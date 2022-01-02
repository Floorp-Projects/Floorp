// The typeof an arrow function is "function".

assertEq(typeof (() => 1), "function");
assertEq(typeof (a => { return a + 1; }), "function");
