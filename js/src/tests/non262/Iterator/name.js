// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
/*---
  The "name" property of Iterator
---*/

const propDesc = Reflect.getOwnPropertyDescriptor(Iterator, 'name');
assertEq(propDesc.value, 'Iterator');
assertEq(propDesc.writable, false);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
