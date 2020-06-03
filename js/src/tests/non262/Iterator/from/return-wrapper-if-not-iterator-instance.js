// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
/*---
  Iterator.from returns an iterator wrapper if O is not an instance of Iterator.
---*/

class TestIterator {
  [Symbol.iterator]() {
    return this;
  }

  next() {
    return { done: false, value: 0 };
  }
}

const iter = new TestIterator();
assertEq(iter instanceof Iterator, false);

const wrapper = Iterator.from(iter);
assertEq(iter !== wrapper, true);
assertEq(wrapper instanceof Iterator, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
