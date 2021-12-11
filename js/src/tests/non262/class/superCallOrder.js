function base() { }

class beforeSwizzle extends base {
    constructor() {
        super(Object.setPrototypeOf(beforeSwizzle, null));
    }
}

new beforeSwizzle();

function MyError() {}

// Again, testing both dynamic prototype dispatch, and that we verify the function
// is a constructor after evaluating args
class beforeThrow extends base {
    constructor() {
        function thrower() { throw new MyError(); }
        super(thrower());
    }
}

Object.setPrototypeOf(beforeThrow, Math.sin);

// Won't throw that Math.sin is not a constructor before evaluating the args
assertThrowsInstanceOf(() => new beforeThrow(), MyError);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
