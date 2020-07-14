// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))
/*---
esid: pending
description: %AsyncIterator.prototype%.filter.name value and descriptor.
info: >
  17 ECMAScript Standard Built-in Objects
features: [iterator-helpers]
---*/

assertEq(AsyncIterator.prototype.filter.name, 'filter');

const propertyDescriptor = Reflect.getOwnPropertyDescriptor(AsyncIterator.prototype.filter, 'name');
assertEq(propertyDescriptor.value, 'filter');
assertEq(propertyDescriptor.enumerable, false);
assertEq(propertyDescriptor.writable, false);
assertEq(propertyDescriptor.configurable, true);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
