// The Map constructor creates an empty Map by default.

load(libdir + "asserts.js");

var m = new Map();
assertEq(m.size, 0);
m = new Map(undefined);
assertEq(m.size, 0);
m = new Map(null);
assertEq(m.size, 0);

assertThrowsInstanceOf(() => Map(), TypeError);
assertThrowsInstanceOf(() => Map(undefined), TypeError);
assertThrowsInstanceOf(() => Map(null), TypeError);
