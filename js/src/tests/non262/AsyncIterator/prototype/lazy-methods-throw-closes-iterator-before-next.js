// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: Calling `throw` on a lazy %AsyncIterator.prototype% method closes the source iterator.
info: >
  Iterator Helpers proposal 2.1.6
features: [iterator-helpers]
---*/

class TestError extends Error {}

class TestIterator extends AsyncIterator {
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
  iter => iter.filter(x => x),
  iter => iter.take(1),
  iter => iter.drop(0),
  iter => iter.asIndexedPairs(),
  iter => iter.flatMap(gen),
];

for (const method of methods) {
  const iter = new TestIterator();
  const iterHelper = method(iter);

  assertEq(iter.closed, false);
  iterHelper.throw(new TestError()).then(
    _ => assertEq(true, false, 'Expected reject.'),
    err => {
      assertEq(err instanceof TestError, true);
      assertEq(iter.closed, true);

      iterHelper.next().then(({done, value}) => {
        assertEq(done, true);
        assertEq(value, undefined);
      });
    },
  );
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
