class base {
    constructor() { }
    method() { this.methodCalled++; }
}

class derived extends base {
    constructor() { super(); this.methodCalled = 0; }

    // Test orderings of various evaluations relative to the superbase

    // Unlike in regular element evaluation, the propVal is evaluated before
    // checking the starting object ([[HomeObject]].[[Prototype]])
    testElem() { super[ruin()]; }

    // The starting object for looking up super.method is determined before
    // ruin() is called.
    testProp() { super.method(ruin()); }

    // The entire super.method property lookup has concluded before the args
    // are evaluated
    testPropCallDeleted() { super.method(()=>delete base.prototype.method); }

    // The starting object for looking up super["prop"] is determined before
    // ruin() is called.
    testElemAssign() { super["prop"] = ruin(); }

    // Test the normal assignment gotchas
    testAssignElemPropValChange() {
        let x = "prop1";
        super[x] = (()=>(x = "prop2", 0))();
        assertEq(this.prop1, 0);
        assertEq(this.prop2, undefined);
    }

    testAssignProp() {
        Object.defineProperty(base.prototype, "piggy",
            {
              configurable: true,
              set() { throw "WEE WEE WEE WEE"; }
            });

        // The property lookup is noted, but not actually evaluated, until the
        // right hand side is. Please don't make the piggy cry.
        super.piggy = (() => delete base.prototype.piggy)();
    }
    testCompoundAssignProp() {
        let getterCalled = false;
        Object.defineProperty(base.prototype, "horse",
            {
              configurable: true,
              get() { getterCalled = true; return "Of course"; },
              set() { throw "NO!"; }
            });
        super.horse += (()=>(delete base.prototype.horse, ", of course!"))();
        assertEq(getterCalled, true);

        // So, is a horse a horse?
        assertEq(this.horse, "Of course, of course!");
    }
}

function ruin() {
    Object.setPrototypeOf(derived.prototype, null);
    return 5;
}

function reset() {
    Object.setPrototypeOf(derived.prototype, base.prototype);
}

let instance = new derived();
assertThrowsInstanceOf(() => instance.testElem(), TypeError);
reset();

instance.testProp();
assertEq(instance.methodCalled, 1);
reset();

instance.testPropCallDeleted();
assertEq(instance.methodCalled, 2);

instance.testElemAssign();
assertEq(instance.prop, 5);
reset();

instance.testAssignElemPropValChange();

instance.testAssignProp();

instance.testCompoundAssignProp();

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
