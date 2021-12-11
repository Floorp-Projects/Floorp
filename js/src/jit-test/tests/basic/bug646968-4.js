load(libdir + "asserts.js");

// Scoping: `x` in the head of a `for (let x...)` loop refers to the loop variable.

assertThrowsInstanceOf(function () {
var s = "", x = {a: 1, b: 2, c: 3};
for (let x in eval('x'))
    s += x;
assertEq(s, "");
}, ReferenceError);
