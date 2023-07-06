// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

//
//
/*---
esid: pending
description: %Iterator.prototype%.flatMap closes the iterator and throws when mapped isn't iterable.
info: >
  Iterator Helpers proposal 2.1.5.7
  1. Repeat,
    ...
    f. Let innerIterator be GetIteratorFlattenable(mapped).
    g. IfAbruptCloseIterator(innerIterator, iterated).
features: [iterator-helpers]
---*/

class InvalidIterable {
  [Symbol.iterator]() {
    return {};
  }
}

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

const nonIterables = [
  new InvalidIterable(),
  undefined,
  null,
  0,
  false,
  Symbol(''),
  0n,
  {},
];

for (const value of nonIterables) {
  const iter = new TestIterator();
  const mapped = iter.flatMap(x => value);

  assertEq(iter.closed, false);
  assertThrowsInstanceOf(() => mapped.next(), TypeError);
  assertEq(iter.closed, true);
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);

