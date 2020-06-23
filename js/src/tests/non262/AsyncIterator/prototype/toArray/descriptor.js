// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 
/*---
  Descriptor property of AsyncIterator.prototype.toArray
---*/

const propDesc = Reflect.getOwnPropertyDescriptor(AsyncIterator.prototype, 'toArray');
assertEq(typeof propDesc.value, 'function');
assertEq(propDesc.writable, true);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
