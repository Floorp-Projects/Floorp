// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) -- AsyncIterator is not enabled unconditionally
/*---
  AsyncIterator constructor throws when called without new.
---*/

assertThrowsInstanceOf(() => AsyncIterator(), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
