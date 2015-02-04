// The WeakMap constructor creates an empty WeakMap by default.

load(libdir + "asserts.js");

new WeakMap();
new WeakMap(undefined);
new WeakMap(null);

// FIXME: bug 1083752
options("werror");
assertEq(evaluate("WeakMap()", {catchTermination: true}), "terminated");
// assertThrowsInstanceOf(() => WeakMap(), TypeError);
// assertThrowsInstanceOf(() => WeakMap(undefined), TypeError);
// assertThrowsInstanceOf(() => WeakMap(null), TypeError);
