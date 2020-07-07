// |reftest| skip-if(!this.hasOwnProperty('Iterator'))
//

/*---
esid: pending
description: TypeError is thrown if `this` is an Array.
info:
features: [Symbol.iterator]
---*/

assertThrowsInstanceOf(() => Iterator.prototype.map.call([], x => x), TypeError);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
