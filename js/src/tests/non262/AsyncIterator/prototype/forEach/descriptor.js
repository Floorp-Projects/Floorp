// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 
/*---
  Descriptor property of AsyncIterator.prototype.forEach
---*/

const propDesc = Reflect.getOwnPropertyDescriptor(AsyncIterator.prototype, 'forEach');
assertEq(typeof propDesc.value, 'function');
assertEq(propDesc.writable, true);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
