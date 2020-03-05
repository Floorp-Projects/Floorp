// Test case adapted from "apply-optimization.js" to test SpreadNew instead of FunApply.

// Uses fewer iterations than "apply-optimization.js" to keep the overall runtime reasonable.
const iterations = 40;

function make(k) {
    var a = new Array(k);
    for ( let i=0 ; i < k ; i++ )
        a[i] = {}
    return a;
}

class Base {
    constructor() {
        // Ensure |new.target| and |this| are correct.
        assertEq(new.target, g);
        assertEq(typeof this, "object");
        assertEq(Object.getPrototypeOf(this), g.prototype);

        // Returns a Number object, because constructor calls need to return an object!
        return new Number(arguments.length);
    }
}

class g extends Base {
    constructor(...args) {
        super(...args);
    }
}

function f(a) {
    var sum = 0;
    for ( let i=0 ; i < iterations ; i++ )
        sum += new g(...a);
    return sum;
}

function time(k, t) {
    var then = Date.now();
    assertEq(t(), iterations*k);
    var now = Date.now();
    return now - then;
}

function p(v) {
    // Uncomment to see timings
    // print(v);
}

f(make(200));

// There is currently a cutoff after 375 where we bailout in order to avoid
// handling very large stack frames.  This slows the operation down by a factor
// of 100 or so.

p(time(374, () => f(make(374))));
p(time(375, () => f(make(375))));
p(time(376, () => f(make(376))));
p(time(377, () => f(make(377))));
