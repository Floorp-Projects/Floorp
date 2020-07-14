// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))
//

/*---
esid: pending
description: %AsyncIterator.prototype%.drop length value and descriptor.
info: >
features: [iterator-helpers]
---*/

assertEq(AsyncIterator.prototype.drop.length, 1);

const propertyDescriptor = Reflect.getOwnPropertyDescriptor(AsyncIterator.prototype.drop, 'length');
assertEq(propertyDescriptor.value, 1);
assertEq(propertyDescriptor.enumerable, false);
assertEq(propertyDescriptor.writable, false);
assertEq(propertyDescriptor.configurable, true);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
