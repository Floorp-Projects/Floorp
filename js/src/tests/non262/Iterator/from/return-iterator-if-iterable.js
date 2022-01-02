// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
/*---
  Iterator.from returns O if it is iterable, an iterator, and an instance of Iterator.
---*/

class TestIterator extends Iterator {
  [Symbol.iterator]() {
    return this;
  }

  next() {
    return { done: false, value: this.value++ };
  }

  value = 0;
}

const iter = new TestIterator();
assertEq(iter, Iterator.from(iter));

const arrayIter = [1, 2, 3][Symbol.iterator]();
assertEq(arrayIter, Iterator.from(arrayIter));

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
