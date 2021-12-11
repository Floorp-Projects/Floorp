// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 
/*---
  `name` property of AsyncIterator.prototype.toArray.
---*/

const propDesc = Reflect.getOwnPropertyDescriptor(AsyncIterator.prototype.toArray, 'name');
assertEq(propDesc.value, 'toArray');
assertEq(propDesc.writable, false);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
