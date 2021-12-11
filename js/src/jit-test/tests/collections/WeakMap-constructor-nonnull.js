// WeakMap constructor should throw when key is nonnull.

load(libdir + "asserts.js");

var v1 = 42;

var primitive = 10;
assertThrowsInstanceOf(() => new WeakMap([[primitive, v1]]), TypeError);

var empty_array = [];
assertThrowsInstanceOf(() => new WeakMap([empty_array]), TypeError);
