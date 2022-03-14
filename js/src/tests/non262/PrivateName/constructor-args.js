class A {
    #x = "hello";
    constructor(o = this.#x) {
        this.value = o;
    }
};

var a = new A;
assertEq(a.value, "hello");


class B extends A {
    constructor() {
        // Cannot access 'this' until super() called.
        super();
        assertEq("value" in this, true);
        assertEq(this.value, "hello");
    }
}

var b = new B;

if (typeof reportCompare === 'function') reportCompare(0, 0);
