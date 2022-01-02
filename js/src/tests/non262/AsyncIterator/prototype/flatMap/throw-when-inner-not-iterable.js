// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: %AsyncIterator.prototype%.flatMap closes the iterator and throws when mapped isn't iterable.
info: >
  Iterator Helpers proposal 2.1.6.7
  1. Repeat,
    ...
    h. Let innerIterator be GetIterator(mapped, async).
    i. IfAbruptCloseAsyncIterator(innerIterator, iterated).
features: [iterator-helpers]
---*/

class NotIterable {
  async next() {
    return {done: true};
  }
}

class InvalidIterable {
  [Symbol.asyncIterator]() {
    return {};
  }
}

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

const nonIterables = [
  new NotIterable(),
  new InvalidIterable(),
  undefined,
  null,
  0,
  false,
  Symbol(''),
  0n,
  {},
];

(async () => {
  for (const value of nonIterables) {
    const iter = new TestIterator();
    const mapped = iter.flatMap(x => value);

    assertEq(iter.closed, false);
    console.log("here!");
    try {
      await mapped.next();
      assertEq(true, false, 'Expected reject');
    } catch (exc) {
      assertEq(exc instanceof TypeError, true);
    }
    assertEq(iter.closed, true);
  }
})();

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
