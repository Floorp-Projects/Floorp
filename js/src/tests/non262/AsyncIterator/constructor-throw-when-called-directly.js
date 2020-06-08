// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))
/*---
  AsyncIterator constructor throws when called directly.
---*/

assertThrowsInstanceOf(() => new AsyncIterator(), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
