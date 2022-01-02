// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
/*---
  `name` property of Iterator.prototype.find.
---*/

const propDesc = Reflect.getOwnPropertyDescriptor(Iterator.prototype.find, 'name');
assertEq(propDesc.value, 'find');
assertEq(propDesc.writable, false);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
