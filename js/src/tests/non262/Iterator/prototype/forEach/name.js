// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
/*---
  `name` property of Iterator.prototype.forEach.
---*/

const propDesc = Reflect.getOwnPropertyDescriptor(Iterator.prototype.forEach, 'name');
assertEq(propDesc.value, 'forEach');
assertEq(propDesc.writable, false);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
