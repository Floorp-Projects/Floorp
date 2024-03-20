// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
/*---
  The prototype of the Iterator constructor is the intrinsic object %FunctionPrototype%.
---*/

assertEq(Object.getPrototypeOf(Iterator), Function.prototype);

const propDesc = Reflect.getOwnPropertyDescriptor(Iterator, 'prototype');
assertEq(propDesc.writable, false);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, false);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
