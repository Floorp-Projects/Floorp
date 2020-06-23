// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 
/*---
  `name` property of AsyncIterator.prototype.some.
---*/

const propDesc = Reflect.getOwnPropertyDescriptor(AsyncIterator.prototype.some, 'name');
assertEq(propDesc.value, 'some');
assertEq(propDesc.writable, false);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
