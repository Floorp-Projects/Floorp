class A {
    constructor() { }
}

class B extends A { }

var b = new B();
assertEq(b.constructor.name, "B");

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
