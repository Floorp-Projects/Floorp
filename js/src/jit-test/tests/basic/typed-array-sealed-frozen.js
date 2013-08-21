// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// Typed arrays are always sealed.

load(libdir + "asserts.js")

const constructors = [
  Int8Array,
  Uint8Array,
  Uint8ClampedArray,
  Int16Array,
  Uint16Array,
  Int32Array,
  Uint32Array,
  Float32Array,
  Float64Array
];

for (constructor of constructors) {
  print("testing non-empty " + constructor.name);
  let a = new constructor(10);
  assertEq(Object.isExtensible(a), false);
  assertEq(Object.isSealed(a), true);
  assertEq(Object.isFrozen(a), false);

  // Should not throw.
  Object.seal(a);

  // Should complain that it can't change attributes of indexed typed array properties.
  assertThrowsInstanceOf(() => Object.freeze(a), InternalError);
}

print();

for (constructor of constructors) {
  print("testing zero-length " + constructor.name);
  let a = new constructor(0);
  assertEq(Object.isExtensible(a), false);
  assertEq(Object.isSealed(a), true);
  assertEq(Object.isFrozen(a), true);

  // Should not throw.
  Object.seal(a);
  Object.freeze(a);
}

// isSealed and isFrozen should not try to build an array of all the
// property names of a typed array, since they're often especially large.
// This should not throw an allocation error.
let a = new Uint8Array(1 << 24);
Object.isSealed(a);
Object.isFrozen(a);
