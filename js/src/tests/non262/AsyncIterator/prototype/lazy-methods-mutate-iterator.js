// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))
//
//

/*---
esid: pending
description: %AsyncIterator.prototype% methods work properly if the iterator has been mutated.
info: >
  Iterator Helpers proposal 2.1.6
features: [iterator-helpers]
---*/

class TestIterator extends AsyncIterator {
  values = [1, 2];
  async next() {
    if (this.values.length == 0)
      return {done: true};
    return {done: false, value: this.values.shift()};
  }
}

function check({done, value}, expectedDone, expectedValue) {
  assertEq(done, expectedDone);
  assertEq(Array.isArray(value) ? value[1] : value, expectedValue);
}

const methods = [
  ['map', x => x],
  ['filter', x => true],
  ['take', Infinity],
  ['drop', 0],
  ['asIndexedPairs', undefined],
  ['flatMap', async function*(x) { yield x; }],
];

for (const [method, arg] of methods) {
  (async () => {
    const iter = new TestIterator();
    const iterHelper = iter[method](arg);
    check(await iterHelper.next(), false, 1);
    check(await iterHelper.next(), false, 2);
    iter.values.push(3);
    check(await iterHelper.next(), false, 3);
    check(await iterHelper.next(), true, undefined);
  })();
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
