// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

//
//
/*---
esid: pending
description: %Iterator.prototype%.take returns if the iterator is done.
info: >
  Iterator Helpers proposal 2.1.5.4
  2. Repeat,
    ...
    c. Let next be ? IteratorStep(iterated, lastValue).
    d. If next is false, return undefined.
features: [iterator-helpers]
---*/

let iter = [1, 2].values().take(3);
for (const expected of [1, 2]) {
  const result = iter.next();
  assertEq(result.value, expected);
  assertEq(result.done, false);
}
let result = iter.next();
assertEq(result.value, undefined);
assertEq(result.done, true);

class TestIterator extends Iterator {
  counter = 0;
  next() {
    return {done: ++this.counter >= 2, value: undefined};
  }

  closed = false;
  return(value) {
    this.closed = true;
    return {done: true, value};
  }
}

iter = new TestIterator();
let taken = iter.take(10);
for (const value of taken) {
  assertEq(value, undefined);
}
result = taken.next();
assertEq(result.value, undefined);
assertEq(result.done, true);
assertEq(iter.counter, 2);
assertEq(iter.closed, false);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
