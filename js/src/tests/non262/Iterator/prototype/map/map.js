// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

/*---
esid: pending
description: %Iterator.prototype%.map value and descriptor.
info: >
  17 ECMAScript Standard Built-in Objects
features: [iterator-helpers]
---*/

const map = Reflect.getOwnPropertyDescriptor(Iterator.prototype, 'map');

assertEq(
  Iterator.prototype.map, map.value,
  'The value of `%Iterator.prototype%.map` is the same as the value in the property descriptor.'
);

assertEq(
  typeof map.value, 'function',
  '%Iterator.prototype%.map is a function.'
);

assertEq(map.enumerable, false);
assertEq(map.writable, true);
assertEq(map.configurable, true);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
