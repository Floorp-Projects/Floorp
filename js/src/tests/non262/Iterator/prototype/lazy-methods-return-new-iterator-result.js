// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

//
//
/*---
esid: pending
description: Lazy Iterator Helper methods return new iterator result objects.
info: >
  Iterator Helpers proposal 2.1.5
features: [iterator-helpers]
---*/

const iterResult = {done: false, value: 1, testProperty: 'test'};
class TestIterator extends Iterator {
  next() {
    return iterResult;
  }
}

const methods = [
  iter => iter.map(x => x),
  iter => iter.filter(x => true),
  iter => iter.take(1),
  iter => iter.drop(0),
  iter => iter.flatMap(x => [x]),
];

// Call `return` before stepping the iterator:
for (const method of methods) {
  const iter = new TestIterator();
  const iterHelper = method(iter);
  const result = iterHelper.next();
  assertEq(result == iterResult, false);
  assertEq(result.testProperty, undefined);
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
