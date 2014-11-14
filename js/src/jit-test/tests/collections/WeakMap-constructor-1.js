// The WeakMap constructor creates an empty WeakMap by default.

load(libdir + "asserts.js");

new WeakMap();
new WeakMap(undefined);

// FIXME: bug 1092538
// new WeakMap(null);

// FIXME: bug 1062075
// assertThrowsInstanceOf(() => WeakMap(), TypeError);
// assertThrowsInstanceOf(() => WeakMap(undefined), TypeError);
// WeakMap(null) throws TypeError, but it's because of bug 1092538.
// assertThrowsInstanceOf(() => WeakMap(null), TypeError);
