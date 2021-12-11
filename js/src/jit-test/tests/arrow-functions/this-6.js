// Eval expressions in arrow functions use the correct |this| value.

function Dog(name) {
    this.name = name;
    this.getName = () => eval("this.name");
    this.getNameHard = () => eval("(() => this.name)()");
}

var d = new Dog("Max");
assertEq(d.getName(), d.name);
assertEq(d.getNameHard(), d.name);
