// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

class TestIterator extends Iterator {
  constructor(value) {
    super();
    this.value = value;
  }

  next() {
    return this.value;
  }
}

const sum = (x, y) => x + y;

let iter = new TestIterator(undefined);
assertThrowsInstanceOf(() => iter.reduce(sum), TypeError);
iter = new TestIterator(null);
assertThrowsInstanceOf(() => iter.reduce(sum), TypeError);
iter = new TestIterator(0);
assertThrowsInstanceOf(() => iter.reduce(sum), TypeError);
iter = new TestIterator(false);
assertThrowsInstanceOf(() => iter.reduce(sum), TypeError);
iter = new TestIterator('');
assertThrowsInstanceOf(() => iter.reduce(sum), TypeError);
iter = new TestIterator(Symbol(''));
assertThrowsInstanceOf(() => iter.reduce(sum), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
