// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

class TestIterator extends Iterator {
  next() {
    throw new Error();
  }

  closed = false;
  return() {
    this.closed = true;
  }
}

const fn = x => x;
const iter = new TestIterator();

assertEq(iter.closed, false);
assertThrowsInstanceOf(() => iter.find(fn), Error);
assertEq(iter.closed, false);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
