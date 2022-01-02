// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: Lazy %AsyncIterator.prototype% methods close the iterator if `yield` throws.
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
  async return(value) {
    this.closed = true;
    return {done: true, value};
  }
}

async function* gen(x) { yield x; }
const methods = [
  iter => iter.map(x => x),
  iter => iter.filter(x => true),
  iter => iter.take(1),
  iter => iter.drop(0),
  iter => iter.asIndexedPairs(),
  iter => iter.flatMap(gen),
];

for (const method of methods) {
  const iterator = new TestAsyncIterator();
  const iteratorHelper = method(iterator);

  assertEq(iterator.closed, false);
  iteratorHelper.next().then(
    _ => iteratorHelper.throw(new TestError()).then(
      _ => assertEq(true, false, 'Expected reject'),
      err => {
        assertEq(err instanceof TestError, true);
        assertEq(iterator.closed, true);
      },
    ),
  );
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
