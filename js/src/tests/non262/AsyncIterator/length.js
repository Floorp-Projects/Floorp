// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) -- AsyncIterator is not enabled unconditionally
/*---
  The "length" property of AsyncIterator
---*/

const propDesc = Reflect.getOwnPropertyDescriptor(AsyncIterator, 'length');
assertEq(propDesc.value, 0);
assertEq(propDesc.writable, false);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
