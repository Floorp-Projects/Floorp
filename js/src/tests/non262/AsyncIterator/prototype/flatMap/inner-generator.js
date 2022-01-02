// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: %AsyncIterator.prototype%.flatMap innerIterator can be a generator.
info: >
  Iterator Helpers proposal 2.1.6.7
features: [iterator-helpers]
---*/

async function* gen() {
  yield 1;
  yield 2;
}

(async () => {
  const iter = gen().flatMap(async function*(x) {
    yield x;
    yield* [x + 1, x + 2];
  });

  for (const expected of [1, 2, 3, 2, 3, 4]) {
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
