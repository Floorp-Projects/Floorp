class base {
    constructor() { }
    test() {
        return false;
    }
}

let standin = { test() { return true; } };

class derived extends base {
    constructor() { super(); }
    test() {
        assertEq(super.test(), false);
        Object.setPrototypeOf(derived.prototype, standin);
        assertEq(super["test"](), true);
    }
}

new derived().test();

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
