// Make sure we get the proper side effects.
// |delete super.prop| and |delete super[expr]| throw universally.

class base {
    constructor() { }
}

class derived extends base {
    constructor() { super(); }
    testDeleteProp() { delete super.prop; }
    testDeleteElem() {
        let sideEffect = 0;
        assertThrowsInstanceOf(() => delete super[sideEffect = 1], ReferenceError);
        assertEq(sideEffect, 1);
    }
    testDeleteElemPropValFirst() {
        // The deletion error is a reference error, but by munging the prototype
        // chain, we can force a typeerror from JSOP_SUPERBASE
        delete super[Object.setPrototypeOf(derived.prototype, null)];
    }
}

var d = new derived();
assertThrowsInstanceOf(() => d.testDeleteProp(), ReferenceError);
d.testDeleteElem();
assertThrowsInstanceOf(() => d.testDeleteElemPropValFirst(), TypeError);

// |delete super.x| does not delete anything before throwing.
var thing1 = {
    go() { delete super.toString; }
};
let saved = Object.prototype.toString;
assertThrowsInstanceOf(() => thing1.go(), ReferenceError);
assertEq(Object.prototype.toString, saved);

// |delete super.x| does not tell the prototype to delete anything, when it's a proxy.
var thing2 = {
    go() { delete super.prop; }
};
Object.setPrototypeOf(thing2, new Proxy({}, {
    deleteProperty(x) { throw "FAIL"; }
}));
assertThrowsInstanceOf(() => thing2.go(), ReferenceError);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
