// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
/*---
  Iterator.from throws when called with an object with a non-callable @@iterator property.
---*/

assertThrowsInstanceOf(() => Iterator.from({ [Symbol.iterator]: 0 }), TypeError);
assertThrowsInstanceOf(() => Iterator.from({ [Symbol.iterator]: false }), TypeError);
assertThrowsInstanceOf(() => Iterator.from({ [Symbol.iterator]: "" }), TypeError);
assertThrowsInstanceOf(() => Iterator.from({ [Symbol.iterator]: {} }), TypeError);
assertThrowsInstanceOf(() => Iterator.from({ [Symbol.iterator]: Symbol('') }), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
