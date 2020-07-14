// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: %AsyncIterator.prototype%.flatMap closes the iterator when IteratorNext throws.
info: >
  Iterator Helpers proposal 2.1.6.7
  1. Repeat,
    ...
    k. Repeat, while innerAlive is true,
      i. Let innerNextPromise be IteratorNext(innerIterator).
      ii. IfAbruptCloseAsyncIterator(innerNextPromise, iterated).
features: [iterator-helpers]
---*/

class TestIterator extends AsyncIterator {
  async next() {
    return {done: false, value: 0};
  }

  closed = false;
  async return(value) {
    this.closed = true;
    return {done: true, value};
  }
}

class TestError extends Error {}
class InnerIterator extends AsyncIterator {
  async next() {
    throw new TestError();
  }
}

const iter = new TestIterator();
const mapped = iter.flatMap(x => new InnerIterator());

assertEq(iter.closed, false);
mapped.next().then(
  _ => assertEq(true, false, 'Expected reject.'),
  err => {
    assertEq(err instanceof TestError, true);
    assertEq(iter.closed, true);
  }
);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
