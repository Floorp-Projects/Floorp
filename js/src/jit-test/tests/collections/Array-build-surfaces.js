// Check superficial features of Array.build.

load(libdir + "asserts.js");

var desc = Object.getOwnPropertyDescriptor(Array, "build");
assertEq(desc.configurable, true);
assertEq(desc.enumerable, false);
assertEq(desc.writable, true);
assertEq(Array.build.length, 2);
assertThrowsInstanceOf(() => new Array.build(), TypeError);  // not a constructor

// Must pass a function to second argument.
for (let v of [undefined, null, false, "cow"])
  assertThrowsInstanceOf(() => Array.build(1, v), TypeError);

// The first argument must be a legal length.
assertThrowsInstanceOf(() => Array.build(-1, function() {}), RangeError);

// When the this-value passed in is not a constructor, the result is an array.
for (let v of [undefined, null, false, "cow"])
  assertEq(Array.isArray(Array.build.call(v, 1, function() {})), true);
