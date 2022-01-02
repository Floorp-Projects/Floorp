// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
/*---
  Property descriptor of Iterator.
---*/

const propDesc = Reflect.getOwnPropertyDescriptor(this, 'Iterator');
assertEq(propDesc.value, Iterator);
assertEq(propDesc.writable, true);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
