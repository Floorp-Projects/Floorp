// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: %AsyncIterator.prototype%.flatMap closes the iterator when innerComplete throws.
info: >
  Iterator Helpers proposal 2.1.6.7
  1. Repeat,
    ...
    k. Repeat, while innerAlive is true,
      ...
      v. Let innerComplete be IteratorComplete(innerNext).
      vi. IfAbruptCloseAsyncIterator(innerComplete, iterated).
features: [iterator-helpers]
---*/

class TestIterator extends AsyncIterator {
  async next() {
    return {done: false, value: 0};
  }

  closed = false;
  async return() {
    this.closed = true;
    return {done: true};
  }
}

class TestError extends Error {}
class InnerIterator extends AsyncIterator {
  async next() {
    return {
      get done() {
        throw new TestError();
      }
    };
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
