// Arguments with default parameters can shadow const locals.

"use strict";

function f() {
    const x = 1;
    return (x = 0) => x;
}

var g = f();
assertEq(g(), 0);
