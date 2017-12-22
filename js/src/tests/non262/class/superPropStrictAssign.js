// While |super| is common in classes, it also works in object litterals,
// where there is no guarantee of strict mode. Check that we do not somehow
// get strict mode semantics when they were not called for

// |undefined|, writable: false
Object.defineProperty(Object.prototype, "prop", { writable: false });

class strictAssignmentTest {
    constructor() {
        // Strict mode. Throws.
        super.prop = 14;
    }
}

assertThrowsInstanceOf(()=>new strictAssignmentTest(), TypeError);

// Non-strict. Silent failure.
({ test() { super.prop = 14; } }).test();

assertEq(Object.prototype.prop, undefined);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
