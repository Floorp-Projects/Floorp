// The completion value of a for-of loop is the completion value of the
// last evaluation of the body, or undefined.

assertEq(eval("for (let x of [1, 2, 3]) { x }"), 3);
assertEq(eval("for (let x of [1, 2, 3]) { x * 2 }"), 6);
assertEq(eval("for (let x of []) { x }"), undefined);
