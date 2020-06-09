// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
/*---
  Descriptor property of Iterator.prototype.forEach
---*/

const propDesc = Reflect.getOwnPropertyDescriptor(Iterator.prototype, 'forEach');
assertEq(typeof propDesc.value, 'function');
assertEq(propDesc.writable, true);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
