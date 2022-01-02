// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
/*---
  Iterator constructor throws when called without new.
---*/

assertThrowsInstanceOf(() => Iterator(), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
