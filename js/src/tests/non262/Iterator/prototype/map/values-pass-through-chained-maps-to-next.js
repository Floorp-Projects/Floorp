// |reftest| skip-if(!this.hasOwnProperty('Iterator'))
//

/*---
esid: pending
description: Multiple chained %Iterator.prototype%.map calls pass `lastValue` to the iterator's `next` call.
info: >
  Iterator Helpers Proposal 2.1.5.2
features: [iterator-helpers]
---*/

let nextWasPassed;

const iteratorWhereNextTakesValue = Object.setPrototypeOf({
  next: function(value) {
    nextWasPassed = value;

    if (this.value < 3)
      return { done: false, value: this.value++ };
    return { done: true, value: undefined };
  },
  value: 0,
}, Iterator.prototype);

const mappedIterator = iteratorWhereNextTakesValue.map(x => 2 * x).map(x => 1 + x);

assertEq(mappedIterator.next(1).value, 1);
assertEq(nextWasPassed, undefined);

assertEq(mappedIterator.next(2).value, 3);
assertEq(nextWasPassed, 2);

assertEq(mappedIterator.next(3).value, 5);
assertEq(nextWasPassed, 3);

assertEq(mappedIterator.next(4).done, true);
assertEq(nextWasPassed, 4);

assertEq(mappedIterator.next(5).done, true);
assertEq(nextWasPassed, 4);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
