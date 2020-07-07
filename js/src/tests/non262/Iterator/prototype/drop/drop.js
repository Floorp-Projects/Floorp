// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

/*---
esid: pending
description: Smoketest of %Iterator.prototype%.drop.
info: >
  Iterator Helpers proposal 2.1.5.5
features: [iterator-helpers]
---*/

let iter = [1, 2, 3].values().drop(1);

for (const v of [2, 3]) {
  let result = iter.next();
  assertEq(result.done, false);
  assertEq(result.value, v);
}

assertEq(iter.next().done, true);

// `drop`, when called without arguments, has a limit of undefined,
// which converts to 0.
assertEq(['test'].values().drop().next().value, 'test');

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
