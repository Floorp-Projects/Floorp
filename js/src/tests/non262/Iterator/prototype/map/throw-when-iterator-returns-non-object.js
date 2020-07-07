// |reftest| skip-if(!this.hasOwnProperty('Iterator'))
//

/*---
esid: pending
description: Throw TypeError if `next` call returns non-object.
info:
features: [iterator-helpers]
---*/

const iterator = returnValue => Object.setPrototypeOf({
  next: () => returnValue,
}, Iterator.prototype);
const mapper = x => x;

assertThrowsInstanceOf(() => iterator(undefined).map(mapper).next(), TypeError);
assertThrowsInstanceOf(() => iterator(null).map(mapper).next(), TypeError);
assertThrowsInstanceOf(() => iterator(0).map(mapper).next(), TypeError);
assertThrowsInstanceOf(() => iterator(false).map(mapper).next(), TypeError);
assertThrowsInstanceOf(() => iterator('').map(mapper).next(), TypeError);
assertThrowsInstanceOf(() => iterator(Symbol()).map(mapper).next(), TypeError);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
