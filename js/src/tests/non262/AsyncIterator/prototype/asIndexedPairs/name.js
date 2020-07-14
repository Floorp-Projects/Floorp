// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

/*---
esid: pending
description: %AsyncIterator.prototype%.asIndexedPairs.name value and descriptor.
info: >
  17 ECMAScript Standard Built-in Objects
features: [iterator-helpers]
---*/

assertEq(AsyncIterator.prototype.asIndexedPairs.name, 'asIndexedPairs');

const propertyDescriptor = Reflect.getOwnPropertyDescriptor(AsyncIterator.prototype.asIndexedPairs, 'name');
assertEq(propertyDescriptor.value, 'asIndexedPairs');
assertEq(propertyDescriptor.enumerable, false);
assertEq(propertyDescriptor.writable, false);
assertEq(propertyDescriptor.configurable, true);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
