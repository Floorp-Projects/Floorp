// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

class TestIterator extends Iterator {
  next() {
    throw new Error();
  }
}

const iter = new TestIterator();

assertThrowsInstanceOf(() => iter.toArray(), Error);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
