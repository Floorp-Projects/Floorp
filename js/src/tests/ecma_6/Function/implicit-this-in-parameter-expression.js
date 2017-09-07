
function f(a = eval(`
    function g() {
        'use strict';
        return this;
    }

    with ({}) {
        g() /* implicit return value */
    }
    `)) {
    return a
};

assertEq(f(), undefined);

if (typeof reportCompare === "function")
    reportCompare(true, true);
