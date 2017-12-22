var BUGNUMBER = 1180290;
var summary = 'ArrayBuffer getters should have get prefix';

print(BUGNUMBER + ": " + summary);

assertEq(Object.getOwnPropertyDescriptor(ArrayBuffer, Symbol.species).get.name, "get [Symbol.species]");
assertEq(Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, "byteLength").get.name, "get byteLength");

if (typeof reportCompare === 'function')
  reportCompare(true, true);
