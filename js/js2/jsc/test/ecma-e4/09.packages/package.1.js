
package Mission {

class Spaceship extends Vehicle {
    var motor : Rocket;
    var velocity ; int;
    var occupancy : int;

    constructor function Spaceship( motor:Rocket ) {
        this.motor = motor;
    }

    function fly() {};
}

class Vehicle {
    function travel() {};
}

function run() {
    var vehicle : Vehicle;

    vehicle = new Spaceship();

    vehicle.travel();
    vehicle.fly();
}

}