// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

class TestIterator extends Iterator {
  next() {
    return { done: false, value: 0 };
  }
}

const iter = new TestIterator();
assertThrowsInstanceOf(() => iter.reduce(), TypeError);
assertThrowsInstanceOf(() => iter.reduce(undefined), TypeError);
assertThrowsInstanceOf(() => iter.reduce(null), TypeError);
assertThrowsInstanceOf(() => iter.reduce(0), TypeError);
assertThrowsInstanceOf(() => iter.reduce(false), TypeError);
assertThrowsInstanceOf(() => iter.reduce(''), TypeError);
assertThrowsInstanceOf(() => iter.reduce(Symbol('')), TypeError);
assertThrowsInstanceOf(() => iter.reduce({}), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
