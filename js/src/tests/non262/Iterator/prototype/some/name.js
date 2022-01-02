// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
/*---
  `name` property of Iterator.prototype.some.
---*/

const propDesc = Reflect.getOwnPropertyDescriptor(Iterator.prototype.some, 'name');
assertEq(propDesc.value, 'some');
assertEq(propDesc.writable, false);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
