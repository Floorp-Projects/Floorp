// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
/*---
  `name` property of Iterator.prototype.reduce.
---*/

const propDesc = Reflect.getOwnPropertyDescriptor(Iterator.prototype.reduce, 'name');
assertEq(propDesc.value, 'reduce');
assertEq(propDesc.writable, false);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
