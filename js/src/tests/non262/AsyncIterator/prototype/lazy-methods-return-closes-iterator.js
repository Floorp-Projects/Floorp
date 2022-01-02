// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: Calling `return` on a lazy %AsyncIterator.prototype% method closes the source iterator.
info: >
  Iterator Helpers proposal 2.1.6
features: [iterator-helpers]
---*/

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

  iterHelper.next().then(() => {
    assertEq(iter.closed, false);
    iterHelper.return(0).then(({done, value}) => {
      assertEq(iter.closed, true);
      assertEq(done, true);
      assertEq(value, 0);
    });
  });
}

for (const method of methods) {
  const iter = new TestIterator();
  const iterHelper = method(iter);

  assertEq(iter.closed, false);
  iterHelper.return(0).then(({done, value}) => {
    assertEq(iter.closed, true);
    assertEq(done, true);
    assertEq(value, 0);
  });
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
