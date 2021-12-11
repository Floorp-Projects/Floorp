// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: Lazy %AsyncIterator.prototype% methods throw if `next.done` throws.
info: >
  AsyncIterator Helpers proposal 2.1.6
features: [iterator-helpers]
---*/

class TestError extends Error {}
class TestAsyncIterator extends AsyncIterator {
  async next() {
    return {
      get done() {
        throw new TestError();
      }
    };
  }

  closed = false;
  async return() {
    this.closed = true;
    return {done: true};
  }
}

async function* gen(x) { yield x; }
const methods = [
  iter => iter.map(x => x),
  iter => iter.filter(x => x),
  iter => iter.take(1),
  iter => iter.drop(0),
  iter => iter.asIndexedPairs(),
  iter => iter.flatMap(gen),
];

for (const method of methods) {
  const iterator = new TestAsyncIterator();
  assertEq(iterator.closed, false);
  method(iterator).next().then(
    _ => assertEq(true, false, 'Expected reject'),
    err => {
      assertEq(err instanceof TestError, true);
      assertEq(iterator.closed, false);
    },
  );
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
