// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

class TestIterator extends Iterator {
  next() {
    return { done: this.closed };
  }

  closed = false;
  return() {
    this.closed = true;
  }
}

const fn = () => { throw new Error(); };
const iter = new TestIterator();

assertEq(iter.closed, false);
assertThrowsInstanceOf(() => iter.some(fn), Error);
assertEq(iter.closed, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
