// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
/*---
  Iterator.from returns an iterator wrapper if O is not an iterable.
---*/

class TestIterator {
  next() {
    return { done: false, value: 0 };
  }
}

const iter = new TestIterator();
assertEq(
  Symbol.iterator in iter,
  false,
  'iter is not an iterable.'
);

const wrapper = Iterator.from(iter);
assertEq(iter !== wrapper, true);
assertEq(
  Symbol.iterator in wrapper,
  true,
  'wrapper is an iterable.'
);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
