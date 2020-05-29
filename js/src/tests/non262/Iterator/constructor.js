// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
/*---
  The Iterator constructor is a built-in function.
---*/

assertEq(typeof Iterator, 'function');

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
