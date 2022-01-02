// |reftest| skip-if(!this.hasOwnProperty('Iterator'))
/*---
esid: pending
description: %Iterator.prototype%.map.name value and descriptor.
info: >
  17 ECMAScript Standard Built-in Objects
features: [iterator-helpers]
---*/

assertEq(Iterator.prototype.map.name, 'map');

const propertyDescriptor = Reflect.getOwnPropertyDescriptor(Iterator.prototype.map, 'name');
assertEq(propertyDescriptor.value, 'map');
assertEq(propertyDescriptor.enumerable, false);
assertEq(propertyDescriptor.writable, false);
assertEq(propertyDescriptor.configurable, true);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
