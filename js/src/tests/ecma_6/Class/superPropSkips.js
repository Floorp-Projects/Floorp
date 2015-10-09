var test = `

// Ensure that super lookups and sets skip over properties on the |this| object.
// That is, super lookups start with the superclass, not the current class.

// The whole point: an empty superclass
class base {
    constructor() { }
}

class derived extends base {
    constructor() { super(); this.prop = "flamingo"; }

    toString() { throw "No!"; }

    testSkipGet() {
        assertEq(super.prop, undefined);
    }

    testSkipDerivedOverrides() {
        assertEq(super["toString"](), Object.prototype.toString.call(this));
    }

    testSkipSet() {
        // since there's no prop on the chain, we should set the data property
        // on the receiver, |this|
        super.prop = "rat";
        assertEq(this.prop, "rat");

        // Since the receiver is the instance, we can overwrite inherited
        // properties of the instance, even non-writable ones, as they could be
        // skipped in the super lookup.
        assertEq(this.nonWritableProp, "pony");
        super.nonWritableProp = "bear";
        assertEq(this.nonWritableProp, "bear");
    }
}

Object.defineProperty(derived.prototype, "nonWritableProp", { writable: false, value: "pony" });

let instance = new derived();
instance.testSkipGet();
instance.testSkipDerivedOverrides();
instance.testSkipSet();

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
