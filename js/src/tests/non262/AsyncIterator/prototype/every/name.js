// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))
/*---
  `name` property of AsyncIterator.prototype.every.
---*/

const propDesc = Reflect.getOwnPropertyDescriptor(AsyncIterator.prototype.every, 'name');
assertEq(propDesc.value, 'every');
assertEq(propDesc.writable, false);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
