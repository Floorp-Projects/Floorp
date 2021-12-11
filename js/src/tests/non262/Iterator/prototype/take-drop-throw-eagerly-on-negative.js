// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

//
//
/*---
esid: pending
description: `take` and `drop` throw eagerly when passed negative numbers, after rounding towards 0.
info: >
  Iterator Helpers proposal 2.1.5.4 and 2.1.5.5
features: [iterator-helpers]
---*/

const iter = [].values();
const methods = [
  value => iter.take(value),
  value => iter.drop(value),
];

for (const method of methods) {
  assertThrowsInstanceOf(() => method(-1), RangeError);
  assertThrowsInstanceOf(() => method(-Infinity), RangeError);

  method(NaN);
  method(-NaN);
  method(-0);
  method(-0.9);
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
