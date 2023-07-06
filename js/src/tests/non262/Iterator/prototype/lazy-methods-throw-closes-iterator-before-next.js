// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

//
//
/*---
esid: pending
description: Calling `.throw()` on a lazy %Iterator.prototype% method closes the source iterator.
info: >
  Iterator Helpers proposal 2.1.5
features: [iterator-helpers]
---*/

class TestError extends Error {}

class TestIterator extends Iterator {
  next() { 
    return {done: false, value: 1};
  }

  closed = false;
  return(value) {
    this.closed = true;
    return {done: true, value};
  }
}

const methods = [
  iter => iter.map(x => x),
  iter => iter.filter(x => x),
  iter => iter.take(1),
  iter => iter.drop(0),
  iter => iter.asIndexedPairs(),
  iter => iter.flatMap(x => [x]),
];

for (const method of methods) {
  const iter = new TestIterator();
  const iterHelper = method(iter);

  assertEq(iter.closed, false);
  assertThrowsInstanceOf(() => iterHelper.throw(new TestError()), TestError);
  assertEq(iter.closed, true);

  const result = iterHelper.next();
  assertEq(result.done, true);
  assertEq(result.value, undefined);
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
