var test = `

class base {
    constructor() { }
    test() {
        return false;
    }
}

let standin = { test() { return true; } };

class derived extends base {
    constructor() { }
    test() {
        assertEq(super.test(), false);
        Object.setPrototypeOf(derived.prototype, standin);
        assertEq(super["test"](), true);
    }
}

// This shouldn't throw, but we don't have |super()| yet.
assertThrowsInstanceOf(() => new derived().test(), TypeError);

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
