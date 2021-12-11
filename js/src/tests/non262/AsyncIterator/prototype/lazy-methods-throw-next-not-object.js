// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: Lazy %AsyncIterator.prototype% methods throw if `next` call returns a non-object.
info: >
  Iterator Helpers proposal 2.1.6
features: [iterator-helpers]
---*/

class TestAsyncIterator extends AsyncIterator {
  async next(value) {
    return value;
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
  for (const value of [undefined, null, 0, false, '', Symbol('')]) {
    const iterator = new TestAsyncIterator();
    assertEq(iterator.closed, false);
    method(iterator).next(value).then(
      _ => assertEq(true, false, 'Expected reject'),
      err => {
        assertEq(err instanceof TypeError, true);
        assertEq(iterator.closed, false);
      },
    );
  }
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
