// The Set constructor creates an empty Set by default.

load(libdir + "asserts.js");

var s = new Set();
assertEq(s.size, 0);
s = new Set(undefined);
assertEq(s.size, 0);
s = new Set(null);
assertEq(s.size, 0);

// FIXME: bug 1083752
options("werror");
assertEq(evaluate("Set()", {catchTermination: true}), "terminated");
// assertThrowsInstanceOf(() => Set(), TypeError);
// assertThrowsInstanceOf(() => Set(undefined), TypeError);
// assertThrowsInstanceOf(() => Set(null), TypeError);
