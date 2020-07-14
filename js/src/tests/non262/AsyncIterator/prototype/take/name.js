// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))
/*---
esid: pending
description: %AsyncIterator.prototype%.take.name value and descriptor.
info: >
  17 ECMAScript Standard Built-in Objects
features: [iterator-helpers]
---*/

assertEq(AsyncIterator.prototype.take.name, 'take');

const propertyDescriptor = Reflect.getOwnPropertyDescriptor(AsyncIterator.prototype.take, 'name');
assertEq(propertyDescriptor.value, 'take');
assertEq(propertyDescriptor.enumerable, false);
assertEq(propertyDescriptor.writable, false);
assertEq(propertyDescriptor.configurable, true);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
