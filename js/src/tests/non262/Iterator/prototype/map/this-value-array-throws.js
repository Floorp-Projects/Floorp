// |reftest| skip-if(!this.hasOwnProperty('Iterator'))
//

/*---
esid: pending
description: TypeError not thrown when `this` is an Array.
info:
features: [Symbol.iterator]
---*/

Iterator.prototype.map.call([], x => x);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
