// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))
//

/*---
esid: pending
description: %AsyncIterator.prototype%.asIndexedPairs length value and descriptor.
info: >
  17 ECMAScript Standard Built-in Objects
features: [iterator-helpers]
---*/

assertEq(AsyncIterator.prototype.asIndexedPairs.length, 0);

const propertyDescriptor = Reflect.getOwnPropertyDescriptor(AsyncIterator.prototype.asIndexedPairs, 'length');
assertEq(propertyDescriptor.value, 0);
assertEq(propertyDescriptor.enumerable, false);
assertEq(propertyDescriptor.writable, false);
assertEq(propertyDescriptor.configurable, true);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
