// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

//
//
/*---
esid: pending
description: %Iterator.prototype%.take closes the iterator when remaining is 0.
info: >
  Iterator Helpers proposal 2.1.5.4
features: [iterator-helpers]
---*/

class TestIterator extends Iterator {
  next() {
    return {done: false, value: 1};
  }

  closed = false;
  return() {
    this.closed = true;
    return {done: true};
  }
}

const iter = new TestIterator();
const iterTake = iter.take(1);

let result = iterTake.next();
assertEq(result.done, false);
assertEq(result.value, 1);
assertEq(iter.closed, false);

result = iterTake.next();
assertEq(result.done, true);
assertEq(result.value, undefined);
assertEq(iter.closed, true);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);

