// Check superficial features of Array.of.

load(libdir + "asserts.js");

var desc = Object.getOwnPropertyDescriptor(Array, "of");
assertEq(desc.configurable, true);
assertEq(desc.enumerable, false);
assertEq(desc.writable, true);
assertEq(Array.of.length, 0);
assertThrowsInstanceOf(() => new Array.of(), TypeError);  // not a constructor

// When the this-value passed in is not a constructor, the result is an array.
for (let v of [undefined, null, false, "cow"])
    assertEq(Array.isArray(Array.of.call(v)), true);
