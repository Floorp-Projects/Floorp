// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) -- AsyncIterator is not enabled unconditionally
/*---
  Property descriptor of AsyncIterator.
---*/

const propDesc = Reflect.getOwnPropertyDescriptor(this, 'AsyncIterator');
assertEq(propDesc.value, AsyncIterator);
assertEq(propDesc.writable, true);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
