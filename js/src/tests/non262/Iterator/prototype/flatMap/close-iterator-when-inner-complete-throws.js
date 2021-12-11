// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

//
//
/*---
esid: pending
description: %Iterator.prototype%.flatMap closes the iterator when innerComplete throws.
info: >
  Iterator Helpers proposal 2.1.5.7
  1. Repeat,
    ...
    i. Repeat, while innerAlive is true,
      iii. Let innerComplete be IteratorComplete(innerNext).
      iv. IfAbruptCloseIterator(innerComplete, iterated).
features: [iterator-helpers]
---*/

class TestIterator extends Iterator {
  next() {
    return {done: false, value: 0};
  }

  closed = false;
  return() {
    this.closed = true;
    return {done: true};
  }
}

class TestError extends Error {}
class InnerIterator extends Iterator {
  next() {
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
assertThrowsInstanceOf(() => mapped.next(), TestError);
assertEq(iter.closed, true);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
