// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: Lazy %AsyncIterator.prototype% methods don't close the iterator if getting `then` throws.
info: >
    AsyncIterator Helpers proposal 2.1.6
features: [iterator-helpers]
---*/

class TestError extends Error {}

class TestIterator extends AsyncIterator {
  next() {
    return {
      get then() {
        throw new TestError();
      }
    };
  }

  closed = false;
  async return(value) {
    this.closed = true;
    return {done: true, value};
  }
}

const methods = [
  ["map", x => x],
  ["filter", x => true],
  ["take", Infinity],
  ["drop", 0],
  ["asIndexedPairs", undefined],
  ["flatMap", async function*(x) { yield x; }],
];

(async () => {
  for (const [method, arg] of methods) {
    const iterator = new TestIterator();
    assertEq(iterator.closed, false);

    try {
      await iterator[method](arg).next();
      assertEq(true, false, 'Expected exception');
    } catch(err) {
      console.log(err);
      assertEq(err instanceof TestError, true);
    }
    assertEq(iterator.closed, false);
  }
})();

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
