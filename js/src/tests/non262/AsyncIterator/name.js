// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) -- AsyncIterator is not enabled unconditionally
/*---
  The "name" property of AsyncIterator
---*/

const propDesc = Reflect.getOwnPropertyDescriptor(AsyncIterator, 'name');
assertEq(propDesc.value, 'AsyncIterator');
assertEq(propDesc.writable, false);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
