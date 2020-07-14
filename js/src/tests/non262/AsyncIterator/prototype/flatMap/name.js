// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))
/*---
esid: pending
description: %AsyncIterator.prototype%.flatMap.name value and descriptor.
info: >
  17 ECMAScript Standard Built-in Objects
features: [iterator-helpers]
---*/

assertEq(AsyncIterator.prototype.flatMap.name, 'flatMap');

const propertyDescriptor = Reflect.getOwnPropertyDescriptor(AsyncIterator.prototype.flatMap, 'name');
assertEq(propertyDescriptor.value, 'flatMap');
assertEq(propertyDescriptor.enumerable, false);
assertEq(propertyDescriptor.writable, false);
assertEq(propertyDescriptor.configurable, true);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
