// The WeakSet constructor creates an empty WeakSet by default.

load(libdir + "asserts.js");

new WeakSet();
new WeakSet(undefined);
new WeakSet(null);

assertThrowsInstanceOf(() => WeakSet(), TypeError);
assertThrowsInstanceOf(() => WeakSet(undefined), TypeError);
assertThrowsInstanceOf(() => WeakSet(null), TypeError);
