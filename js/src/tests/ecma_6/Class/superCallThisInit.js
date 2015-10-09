var test = `

function base() { this.prop = 42; }

class testInitialize extends base {
    constructor() {
        // A poor man's assertThrowsInstanceOf, as arrow functions are currently
        // disabled in this context
        try {
            this;
            throw new Error();
        } catch (e if e instanceof ReferenceError) { }
        super();
        assertEq(this.prop, 42);
    }
}
assertEq(new testInitialize().prop, 42);

// super() twice is a no-go.
class willThrow extends base {
    constructor() {
        super();
        super();
    }
}
assertThrowsInstanceOf(()=>new willThrow(), ReferenceError);

// This is determined at runtime, not the syntax level.
class willStillThrow extends base {
    constructor() {
        for (let i = 0; i < 3; i++) {
            super();
        }
    }
}
assertThrowsInstanceOf(()=>new willStillThrow(), ReferenceError);

class canCatchThrow extends base {
    constructor() {
        super();
        try { super(); } catch(e) { }
    }
}
assertEq(new canCatchThrow().prop, 42);

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
