// |reftest| skip-if(!this.hasOwnProperty('Iterator'))
/*---
esid: pending
description: %Iterator.prototype%.drop.name value and descriptor.
info: >
  17 ECMAScript Standard Built-in Objects
features: [iterator-helpers]
---*/

assertEq(Iterator.prototype.drop.name, 'drop');

const propertyDescriptor = Reflect.getOwnPropertyDescriptor(Iterator.prototype.drop, 'name');
assertEq(propertyDescriptor.value, 'drop');
assertEq(propertyDescriptor.enumerable, false);
assertEq(propertyDescriptor.writable, false);
assertEq(propertyDescriptor.configurable, true);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
