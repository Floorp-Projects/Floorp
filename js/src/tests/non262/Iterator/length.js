// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
/*---
  The "length" property of Iterator
---*/

const propDesc = Reflect.getOwnPropertyDescriptor(Iterator, 'length');
assertEq(propDesc.value, 0);
assertEq(propDesc.writable, false);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
