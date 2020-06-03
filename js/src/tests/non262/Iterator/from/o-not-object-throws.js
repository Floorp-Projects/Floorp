// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
/*---
  Iterator.from throws when called with a non-object.
---*/

assertThrowsInstanceOf(() => Iterator.from(undefined), TypeError);
assertThrowsInstanceOf(() => Iterator.from(null), TypeError);
assertThrowsInstanceOf(() => Iterator.from(0), TypeError);
assertThrowsInstanceOf(() => Iterator.from(false), TypeError);
assertThrowsInstanceOf(() => Iterator.from(Symbol('')), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
