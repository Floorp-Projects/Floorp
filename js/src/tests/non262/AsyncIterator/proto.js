// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) -- AsyncIterator is not enabled unconditionally
/*---
  The prototype of the AsyncIterator constructor is the intrinsic object %FunctionPrototype%.
---*/

assertEq(Object.getPrototypeOf(AsyncIterator), Function.prototype);

const propDesc = Reflect.getOwnPropertyDescriptor(AsyncIterator, 'prototype');
assertEq(propDesc.writable, false);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, false);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
