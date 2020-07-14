// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: %AsyncIterator.prototype%.flatMap skips empty inner iterables.
info: >
  Iterator Helpers proposal 2.1.6.7
  1. Repeat,
    ...
    k. Repeat, while innerAlive is true,
      ...
      v. Let innerComplete be IteratorComplete(innerNext).
      ...
      vii. If innerComplete is true, set innerAlive to false.
features: [iterator-helpers]
---*/

async function* gen(values) {
  yield* values;
}

(async () => {
  let iter = gen([0, 1, 2, 3]).flatMap(x => x % 2 ? gen([]) : gen([x]));

  for (const expected of [0, 2]) {
    const result = await iter.next();
    assertEq(result.value, expected);
    assertEq(result.done, false);
  }

  let result = await iter.next();
  assertEq(result.value, undefined);
  assertEq(result.done, true);

  iter = gen([0, 1, 2, 3]).flatMap(x => gen([]));
  result = await iter.next();
  assertEq(result.value, undefined);
  assertEq(result.done, true);
})();

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
