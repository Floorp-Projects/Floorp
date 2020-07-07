// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

//
//
/*---
esid: pending
description: %Iterator.prototype%.flatMap skips empty inner iterables.
info: >
  Iterator Helpers proposal 2.1.5.7
  1. Repeat,
    ...
    i. Repeat, while innerAlive is true,
      ...
      iii. Let innerComplete be IteratorComplete(innerNext).
      ...
      v. If innerComplete is true, set innerAlive to false.
features: [iterator-helpers]
---*/

let iter = [0, 1, 2, 3].values().flatMap(x => x % 2 ? [] : [x]);

for (const expected of [0, 2]) {
  const result = iter.next();
  assertEq(result.value, expected);
  assertEq(result.done, false);
}

let result = iter.next();
assertEq(result.value, undefined);
assertEq(result.done, true);

iter = [0, 1, 2, 3].values().flatMap(x => []);
result = iter.next();
assertEq(result.value, undefined);
assertEq(result.done, true);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
