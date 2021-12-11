// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: Lazy %AsyncIterator.prototype% methods close the iterator if callback throws.
info: >
  AsyncIterator Helpers proposal 2.1.6
features: [iterator-helpers]
---*/

class TestError extends Error {}
class TestAsyncIterator extends AsyncIterator {
  async next() {
    return {done: false, value: 1};
  }

  closed = false;
  async return() {
    this.closed = true;
    return {done: true};
  }
}

function fn() {
  throw new TestError();
}
const methods = [
  iter => iter.map(fn),
  iter => iter.filter(fn),
  iter => iter.flatMap(fn),
];

for (const method of methods) {
  const iter = new TestAsyncIterator();
  assertEq(iter.closed, false);
  method(iter).next().then(
    _ => assertEq(true, false, 'Expected reject'),
    err => {
      assertEq(err instanceof TestError, true);
      assertEq(iter.closed, true);
    },
  );
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);

