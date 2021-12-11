// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: Lazy %AsyncIterator.prototype% method calls can be interleaved.
info: >
  Iterator Helpers proposal 2.1.6
features: [iterator-helpers]
---*/

class TestIterator extends AsyncIterator {
  value = 0;
  async next() { 
    return {done: false, value: this.value++};
  }
}

function unwrapResult(value) {
  // Unwrap the asIndexedPair return values.
  while (Array.isArray(value)) {
    value = value[1];
  }
  return value;
}

function check({done, value}, expectedDone, expectedValue) {
  assertEq(done, expectedDone);
  assertEq(unwrapResult(value), expectedValue);
}

const methods = [
  ['map', x => x],
  ['filter', x => true],
  ['take', Infinity],
  ['drop', 0],
  ['asIndexedPairs', undefined],
  ['flatMap', async function*(x) { yield x; }],
];

(async () => {
  for (const [firstMethod, firstArg] of methods) {
    for (const [secondMethod, secondArg] of methods) {
      const iterator = new TestIterator();

      const firstHelper = iterator[firstMethod](firstArg);
      const secondHelper = iterator[secondMethod](secondArg);

      check(await firstHelper.next(), false, 0);
      check(await secondHelper.next(), false, 1);
      check(await firstHelper.next(), false, 2);
      check(await secondHelper.next(), false, 3);
    }
  }
})();

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
