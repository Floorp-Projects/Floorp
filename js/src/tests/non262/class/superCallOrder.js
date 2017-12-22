function base() { }

class beforeSwizzle extends base {
    constructor() {
        super(Object.setPrototypeOf(beforeSwizzle, null));
    }
}

new beforeSwizzle();

// Again, testing both dynamic prototype dispatch, and that we get the function
// before evaluating args
class beforeThrow extends base {
    constructor() {
        function thrower() { throw new Error(); }
        super(thrower());
    }
}

Object.setPrototypeOf(beforeThrow, Math.sin);

// Will throw that Math.sin is not a constructor before evaluating the args
assertThrowsInstanceOf(() => new beforeThrow(), TypeError);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
