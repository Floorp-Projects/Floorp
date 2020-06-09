// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

class TestIterator extends Iterator {
  next() {
    return { done: this.closed, value: undefined };
  }

  closed = false;
  return() {
    this.closed = true;
  }
}

const reducer = (x, y) => { throw new Error(); };
const iter = new TestIterator();

assertEq(iter.closed, false);
assertThrowsInstanceOf(() => iter.reduce(reducer), Error);
assertEq(iter.closed, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
