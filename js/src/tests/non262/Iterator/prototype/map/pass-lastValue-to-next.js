// |reftest| skip-if(!this.hasOwnProperty('Iterator'))
//

/*---
esid: pending
description: %Iterator.prototype%.map passes lastValue to the `next` call.
info: >
  Iterator Helpers Proposal 2.1.5.2
features: [iterator-helpers]
---*/

const iteratorWhereNextTakesValue = Object.setPrototypeOf({
  next: function(value) {
    assertEq(arguments.length, 0);

    if (this.value < 3)
      return { done: false, value: this.value++ };
    return { done: true, value: undefined };
  },
  value: 0,
}, Iterator.prototype);

const mappedIterator = iteratorWhereNextTakesValue.map(x => x);

assertEq(mappedIterator.next(1).value, 0);

assertEq(mappedIterator.next(2).value, 1);

assertEq(mappedIterator.next(3).value, 2);

assertEq(mappedIterator.next(4).done, true);

assertEq(mappedIterator.next(5).done, true);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
