var BUGNUMBER = 1180290;
var summary = 'Array getters should have get prefix';

print(BUGNUMBER + ": " + summary);

assertEq(Object.getOwnPropertyDescriptor(Array, Symbol.species).get.name, "get [Symbol.species]");

if (typeof reportCompare === 'function')
  reportCompare(true, true);
