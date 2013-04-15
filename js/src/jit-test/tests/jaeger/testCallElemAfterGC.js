// vim: set ts=8 sts=4 et sw=4 tw=99:

function A() {
    this.x = 12;
    this.y = function () { return this.x; };
    this[1] = function () { return this.x; };
}

function f(obj, key){
    assertEq(obj[key](), 12);
}

a = new A();
f(a, "y");
f(a, "y");
f(a, 1);
gc();
f(a, "y");
f(a, "y");

