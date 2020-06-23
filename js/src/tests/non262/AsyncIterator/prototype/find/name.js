// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 
/*---
  `name` property of AsyncIterator.prototype.find.
---*/

const propDesc = Reflect.getOwnPropertyDescriptor(AsyncIterator.prototype.find, 'name');
assertEq(propDesc.value, 'find');
assertEq(propDesc.writable, false);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
