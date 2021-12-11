// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

/*---
esid: pending
description: Smoketest of %Iterator.prototype%.take.
info: >
  Iterator Helpers proposal 2.1.5.4
features: [iterator-helpers]
---*/

let iter = [1, 2, 3].values().take(2);

for (const v of [1, 2]) {
  let result = iter.next();
  assertEq(result.done, false);
  assertEq(result.value, v);
}

assertEq(iter.next().done, true);

// `take`, when called without arguments, has a limit of undefined,
// which converts to 0.
assertEq(['test'].values().take().next().done, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
