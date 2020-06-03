// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
/*---
  `name` property of Iterator.from.
---*/

const propDesc = Reflect.getOwnPropertyDescriptor(Iterator.from, 'name');
assertEq(propDesc.value, 'from');
assertEq(propDesc.writable, false);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
