// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: %AsyncIterator.prototype%.drop returns if the iterator is done.
info: >
  Iterator Helpers proposal 2.1.6.5
  1. Repeat, while remaining > 0,
    ...
    b. Let next be ? Await(? IteratorStep(iterated)).
    c. If ? IteratorComplete(next) is true, return undefined.
features: [iterator-helpers]
---*/

class TestIterator extends AsyncIterator {
  counter = 0;
  async next() {
    return {done: ++this.counter >= 2, value: undefined};
  }
}

(async () => {
  let iter = [1, 2].values().drop(3);
  let result = await iter.next();
  assertEq(result.value, undefined);
  assertEq(result.done, true);

  iter = new TestIterator();
  let dropped = iter.drop(10);
  result = await dropped.next();
  assertEq(result.value, undefined);
  assertEq(result.done, true);
  assertEq(iter.counter, 2);
})();

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
