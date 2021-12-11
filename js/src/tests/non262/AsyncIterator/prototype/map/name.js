// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))
/*---
esid: pending
description: %AsyncIterator.prototype%.map.name value and descriptor.
info: >
  17 ECMAScript Standard Built-in Objects
features: [iterator-helpers]
---*/

assertEq(AsyncIterator.prototype.map.name, 'map');

const propertyDescriptor = Reflect.getOwnPropertyDescriptor(AsyncIterator.prototype.map, 'name');
assertEq(propertyDescriptor.value, 'map');
assertEq(propertyDescriptor.enumerable, false);
assertEq(propertyDescriptor.writable, false);
assertEq(propertyDescriptor.configurable, true);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
