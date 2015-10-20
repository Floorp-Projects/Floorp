// Scoping: `x` in the head of a `for (let x...)` loop refers to the loop variable.

// For now, this means it evaluates to undefined. It ought to throw a
// ReferenceError instead, but the TDZ isn't implemented here (bug 1069480).
var s = "", x = {a: 1, b: 2, c: 3};
for (let x in eval('x'))
    s += x;
assertEq(s, "");
