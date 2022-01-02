// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) -- AsyncIterator is not enabled unconditionally
/*---
  The AsyncIterator constructor is a built-in function.
---*/

assertEq(typeof AsyncIterator, 'function');

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
