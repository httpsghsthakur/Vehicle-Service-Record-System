#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <iomanip>
#include <cmath>
#include <chrono>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <memory>
#include <fstream>

// ==================== CONSTANTS & ENUMS ====================
const double CAR_HOURLY_RATE = 20.0;
const double BIKE_HOURLY_RATE = 10.0;
const double DAILY_MAX = 200.0;
const double MIN_CHARGE_HOURS = 1.0;

enum class VehicleType { CAR, BIKE, HANDICAPPED, ELECTRIC };
enum class SlotStatus { FREE, OCCUPIED, RESERVED, MAINTENANCE };

// ==================== VEHICLE CLASSES ======================
class Vehicle {
protected:
    std::string registration;
    VehicleType type;

public:
    Vehicle(const std::string& reg, VehicleType t) : registration(reg), type(t) {}
    virtual ~Vehicle() = default;

    std::string getRegistration() const { return registration; }
    VehicleType getType() const { return type; }
    virtual double getHourlyRate() const = 0;
    virtual std::string getTypeString() const = 0;
};

class Car : public Vehicle {
public:
    Car(const std::string& reg) : Vehicle(reg, VehicleType::CAR) {}
    double getHourlyRate() const override { return CAR_HOURLY_RATE; }
    std::string getTypeString() const override { return "Car"; }
};

class Bike : public Vehicle {
public:
    Bike(const std::string& reg) : Vehicle(reg, VehicleType::BIKE) {}
    double getHourlyRate() const override { return BIKE_HOURLY_RATE; }
    std::string getTypeString() const override { return "Bike"; }
};

class ElectricCar : public Vehicle {
public:
    ElectricCar(const std::string& reg) : Vehicle(reg, VehicleType::ELECTRIC) {}
    double getHourlyRate() const override { return CAR_HOURLY_RATE * 0.8; }
    std::string getTypeString() const override { return "Electric Car"; }
};

class HandicappedVehicle : public Vehicle {
public:
    HandicappedVehicle(const std::string& reg, VehicleType baseType)
        : Vehicle(reg, baseType) {}

    double getHourlyRate() const override {
        return (type == VehicleType::CAR) ? CAR_HOURLY_RATE * 0.5 : BIKE_HOURLY_RATE * 0.5;
    }

    std::string getTypeString() const override {
        return (type == VehicleType::CAR) ? "Handicapped Car" : "Handicapped Bike";
    }
};

// ==================== PARKING SLOT ====================
class ParkingSlot {
private:
    int id, floor;
    SlotStatus status;
    VehicleType allowedType;
    std::unique_ptr<Vehicle> currentVehicle;
    std::chrono::system_clock::time_point occupiedSince;

public:
    ParkingSlot(int slotId, int flr, VehicleType type)
        : id(slotId), floor(flr), status(SlotStatus::FREE), allowedType(type) {}

    int getId() const { return id; }
    int getFloor() const { return floor; }
    SlotStatus getStatus() const { return status; }

    bool isCompatible(VehicleType vehicleType) const {
        return status == SlotStatus::FREE && allowedType == vehicleType;
    }

    bool parkVehicle(std::unique_ptr<Vehicle> vehicle) {
        if (!isCompatible(vehicle->getType())) return false;
        currentVehicle = std::move(vehicle);
        status = SlotStatus::OCCUPIED;
        occupiedSince = std::chrono::system_clock::now();
        return true;
    }

    std::unique_ptr<Vehicle> vacate() {
        auto vehicle = std::move(currentVehicle);
        status = SlotStatus::FREE;
        return vehicle;
    }

    const Vehicle* getCurrentVehicle() const { return currentVehicle.get(); }
};

// ==================== TICKET ====================
class Ticket {
private:
    int id, floor, slotId;
    std::string vehicleReg;
    VehicleType vehicleType;
    std::chrono::system_clock::time_point entryTime, exitTime;
    bool isActive;

public:
    Ticket(int ticketId, const std::string& reg, VehicleType type, int flr, int slot)
        : id(ticketId), vehicleReg(reg), vehicleType(type),
          floor(flr), slotId(slot), entryTime(std::chrono::system_clock::now()), isActive(true) {}

    int getId() const { return id; }
    std::string getVehicleReg() const { return vehicleReg; }
    VehicleType getVehicleType() const { return vehicleType; }
    int getFloor() const { return floor; }
    int getSlotId() const { return slotId; }
    bool getIsActive() const { return isActive; }

    void exit() {
        exitTime = std::chrono::system_clock::now();
        isActive = false;
    }

    double getParkingDuration() const {
        auto endTime = isActive ? std::chrono::system_clock::now() : exitTime;
        return std::chrono::duration<double>(endTime - entryTime).count() / 3600.0;
    }

    std::string getFormattedEntryTime() const {
        std::time_t time = std::chrono::system_clock::to_time_t(entryTime);
        std::tm* tm = std::localtime(&time);
        std::stringstream ss;
        ss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
};

// ==================== PARKING FLOOR ====================
class ParkingFloor {
private:
    int floorNumber;
    std::vector<ParkingSlot> slots;
    int occupiedSlots = 0;

public:
    ParkingFloor(int floorNum, int carSlots, int bikeSlots) : floorNumber(floorNum) {
        int id = 1;
        for (int i = 0; i < carSlots; ++i)
            slots.emplace_back(id++, floorNum, VehicleType::CAR);
        for (int i = 0; i < bikeSlots; ++i)
            slots.emplace_back(id++, floorNum, VehicleType::BIKE);
    }

    ParkingSlot* findAvailableSlot(VehicleType type) {
        for (auto& slot : slots)
            if (slot.isCompatible(type)) return &slot;
        return nullptr;
    }

    bool parkVehicle(int slotId, std::unique_ptr<Vehicle> vehicle) {
        for (auto& slot : slots)
            if (slot.getId() == slotId && slot.parkVehicle(std::move(vehicle))) {
                occupiedSlots++;
                return true;
            }
        return false;
    }

    std::unique_ptr<Vehicle> vacateSlot(int slotId) {
        for (auto& slot : slots)
            if (slot.getId() == slotId && slot.getStatus() == SlotStatus::OCCUPIED) {
                occupiedSlots--;
                return slot.vacate();
            }
        return nullptr;
    }

    int getOccupiedSlots() const { return occupiedSlots; }
    int getTotalSlots() const { return slots.size(); }
};

// ==================== PARKING SYSTEM ====================
class ParkingSystem {
private:
    std::vector<ParkingFloor> floors;
    std::map<std::string, std::shared_ptr<Ticket>> activeTickets;
    int ticketCounter = 1000;
    double totalRevenue = 0;

public:
    ParkingSystem(int numFloors, int carsPerFloor, int bikesPerFloor) {
        for (int i = 1; i <= numFloors; ++i)
            floors.emplace_back(i, carsPerFloor, bikesPerFloor);
    }

    void parkVehicle();
    void unparkVehicle();
    void displayStatus() const;
};

// ==================== METHODS ====================
void ParkingSystem::parkVehicle() {
    std::string reg;
    int typeChoice;

    std::cout << "\n--- PARK VEHICLE ---\n";
    std::cout << "1. Car ($20/hr)\n2. Bike ($10/hr)\nSelect type: ";
    std::cin >> typeChoice;
    std::cout << "Enter Registration Number: ";
    std::cin >> reg;

    std::unique_ptr<Vehicle> vehicle;
    if (typeChoice == 1) vehicle = std::make_unique<Car>(reg);
    else vehicle = std::make_unique<Bike>(reg);

    for (auto& floor : floors) {
        auto slot = floor.findAvailableSlot(vehicle->getType());
        if (slot && floor.parkVehicle(slot->getId(), std::move(vehicle))) {
            auto ticket = std::make_shared<Ticket>(++ticketCounter, reg,
                slot->getCurrentVehicle()->getType(), slot->getFloor(), slot->getId());
            activeTickets[reg] = ticket;
            std::cout << "Vehicle parked. Ticket ID: " << ticket->getId() << "\n";
            return;
        }
    }
    std::cout << "No slots available.\n";
}

void ParkingSystem::unparkVehicle() {
    std::string reg;
    std::cout << "\n--- UNPARK VEHICLE ---\nEnter Registration Number: ";
    std::cin >> reg;

    auto it = activeTickets.find(reg);
    if (it == activeTickets.end()) {
        std::cout << "Vehicle not found.\n";
        return;
    }

    auto ticket = it->second;
    ticket->exit();
    double hours = std::ceil(ticket->getParkingDuration());
    double rate = (ticket->getVehicleType() == VehicleType::CAR) ? CAR_HOURLY_RATE : BIKE_HOURLY_RATE;
    double charge = std::min(hours * rate, DAILY_MAX);
    totalRevenue += charge;

    floors[ticket->getFloor() - 1].vacateSlot(ticket->getSlotId());
    activeTickets.erase(it);

    std::cout << "Parking charge: $" << charge << "\n";
}

void ParkingSystem::displayStatus() const {
    int total = 0, occ = 0;
    for (const auto& f : floors) {
        total += f.getTotalSlots();
        occ += f.getOccupiedSlots();
    }
    std::cout << "\nTotal Slots: " << total << "\nOccupied: " << occ
              << "\nAvailable: " << total - occ << "\n";
}

// ==================== MAIN ====================
void displayMenu() {
    std::cout << "\n===== SMART PARKING SYSTEM =====\n";
    std::cout << "1. Park Vehicle\n2. Unpark Vehicle\n3. View Status\n4. Exit\nSelect option: ";
}

int main() {
    ParkingSystem parking(3, 10, 5);
    int choice;

    std::cout << "Welcome to Smart Parking System\n";

    while (true) {
        displayMenu();
        std::cin >> choice;
        if (choice == 1) parking.parkVehicle();
        else if (choice == 2) parking.unparkVehicle();
        else if (choice == 3) parking.displayStatus();
        else break;
    }
}
