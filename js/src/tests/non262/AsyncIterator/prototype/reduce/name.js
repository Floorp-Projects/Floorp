// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 
/*---
  `name` property of AsyncIterator.prototype.reduce.
---*/

const propDesc = Reflect.getOwnPropertyDescriptor(AsyncIterator.prototype.reduce, 'name');
assertEq(propDesc.value, 'reduce');
assertEq(propDesc.writable, false);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
