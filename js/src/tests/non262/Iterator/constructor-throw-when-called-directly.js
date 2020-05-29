// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
/*---
  Iterator constructor throws when called directly.
---*/

assertThrowsInstanceOf(() => new Iterator(), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
