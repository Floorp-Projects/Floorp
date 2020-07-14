// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: %AsyncIterator.prototype%.take returns if the iterator is done.
info: >
  Iterator Helpers proposal 2.1.6.4
  2. Repeat,
    ...
    c. Let next be ? Await(? IteratorNext(iterated, lastValue)).
    d. If ? IteratorComplete(next) is false, return undefined.
features: [iterator-helpers]
---*/

async function* gen(values) {
  yield* values;
}

(async () => {
  const iter = gen([1, 2]).take(10);
  for (const expected of [1, 2]) {
    const result = await iter.next();
    assertEq(result.value, expected);
    assertEq(result.done, false);
  }
  const result = await iter.next();
  assertEq(result.value, undefined);
  assertEq(result.done, true);
})();

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
