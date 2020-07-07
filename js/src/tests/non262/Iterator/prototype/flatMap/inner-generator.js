// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

//
//
/*---
esid: pending
description: %Iterator.prototype%.flatMap innerIterator can be a generator.
info: >
  Iterator Helpers proposal 2.1.5.7
features: [iterator-helpers]
---*/

const iter = [1, 2].values().flatMap(function*(x) {
  yield x;
  yield* [x + 1, x + 2];
});

for (const expected of [1, 2, 3, 2, 3, 4]) {
  const result = iter.next();
  assertEq(result.value, expected);
  assertEq(result.done, false);
}

const result = iter.next();
assertEq(result.value, undefined);
assertEq(result.done, true);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
