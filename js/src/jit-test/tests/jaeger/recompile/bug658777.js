function Employee(name, dept) this.name = name || "";
function WorkerBee(name, dept, projs) {
    this.base = Employee
    this.base(name, dept)
}
function Engineer(name, projs, machine) {
    this.base = WorkerBee
    this.base(name, "engineering", projs)
    __proto__["a" + constructor] = 1
}
new Engineer;
