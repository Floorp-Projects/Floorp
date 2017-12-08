var BUGNUMBER = 1180290;
var summary = 'TypedArray getters should have get prefix';

print(BUGNUMBER + ": " + summary);

let TypedArray = Object.getPrototypeOf(Float32Array.prototype).constructor;

assertEq(Object.getOwnPropertyDescriptor(TypedArray, Symbol.species).get.name, "get [Symbol.species]");
assertEq(Object.getOwnPropertyDescriptor(TypedArray.prototype, "buffer").get.name, "get buffer");
assertEq(Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteLength").get.name, "get byteLength");
assertEq(Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteOffset").get.name, "get byteOffset");
assertEq(Object.getOwnPropertyDescriptor(TypedArray.prototype, "length").get.name, "get length");
assertEq(Object.getOwnPropertyDescriptor(TypedArray.prototype, Symbol.toStringTag).get.name, "get [Symbol.toStringTag]");

if (typeof reportCompare === 'function')
  reportCompare(true, true);
