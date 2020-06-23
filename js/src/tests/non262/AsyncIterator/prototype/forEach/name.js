// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 
/*---
  `name` property of AsyncIterator.prototype.forEach.
---*/

const propDesc = Reflect.getOwnPropertyDescriptor(AsyncIterator.prototype.forEach, 'name');
assertEq(propDesc.value, 'forEach');
assertEq(propDesc.writable, false);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
