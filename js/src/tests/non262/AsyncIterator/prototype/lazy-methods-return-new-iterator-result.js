// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: Lazy Iterator Helper methods return new iterator result objects.
info: >
  Iterator Helpers proposal 2.1.6
features: [iterator-helpers]
---*/

const iterResult = {done: false, value: 1, testProperty: 'test'};
class TestIterator extends AsyncIterator {
  async next() {
    return iterResult;
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

// Call `return` before stepping the iterator:
for (const method of methods) {
  const iter = new TestIterator();
  const iterHelper = method(iter);
  iterHelper.next().then(result => {
    assertEq(result == iterResult, false);
    assertEq(result.testProperty, undefined);
  });
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
