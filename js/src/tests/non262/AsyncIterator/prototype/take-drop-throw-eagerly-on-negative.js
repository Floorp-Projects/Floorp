// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: `take` and `drop` throw eagerly when passed negative numbers, after rounding towards 0.
info: >
  Iterator Helpers proposal 2.1.6.4 and 2.1.6.5
features: [iterator-helpers]
---*/

async function* gen() {}
const iter = gen();
const methods = [
  value => iter.take(value),
  value => iter.drop(value),
];

for (const method of methods) {
  assertThrowsInstanceOf(() => method(-1), RangeError);
  assertThrowsInstanceOf(() => method(-Infinity), RangeError);

  // Both -0 and -0.9 round towards 0.
  method(-0);
  method(-0.9);
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
