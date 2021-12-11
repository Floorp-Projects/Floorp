// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))
//

/*---
esid: pending
description: %AsyncIterator.prototype%.filter length value and descriptor.
info: >
  17 ECMAScript Standard Built-in Objects
features: [iterator-helpers]
---*/

assertEq(AsyncIterator.prototype.filter.length, 1);

const propertyDescriptor = Reflect.getOwnPropertyDescriptor(AsyncIterator.prototype.filter, 'length');
assertEq(propertyDescriptor.value, 1);
assertEq(propertyDescriptor.enumerable, false);
assertEq(propertyDescriptor.writable, false);
assertEq(propertyDescriptor.configurable, true);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
