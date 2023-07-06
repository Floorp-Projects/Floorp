// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

//
//
/*---
esid: pending
description: Lazy %Iterator.prototype% methods don't close the iterator if `.next` call throws.
info: >
  Iterator Helpers proposal 2.1.5
features: [iterator-helpers]
---*/

class TestError extends Error {}
class TestIterator extends Iterator {
  next() {
    throw new TestError();
  }

  closed = false;
  return() {
    this.closed = true;
    return {done: true};
  }
}

const iterator = new TestIterator();

const methods = [
  iter => iter.map(x => x),
  iter => iter.filter(x => x),
  iter => iter.take(1),
  iter => iter.drop(0),
  iter => iter.flatMap(x => [x]),
];

for (const method of methods) {
  assertEq(iterator.closed, false);
  assertThrowsInstanceOf(() => method(iterator).next(), TestError);
  assertEq(iterator.closed, false);
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
