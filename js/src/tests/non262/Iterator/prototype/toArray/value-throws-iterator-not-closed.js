// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

class TestError extends Error {}
class TestIterator extends Iterator {
  next() {
    return new Proxy({done: false}, {get: (target, key, receiver) => {
      if (key === 'value')
        throw new TestError();
      return 0;
    }});
  }

  closed = false;
  return() {
    closed = true;
  }
}

const iterator = new TestIterator();
assertEq(iterator.closed, false, 'iterator starts unclosed');
assertThrowsInstanceOf(() => iterator.toArray(), TestError);
assertEq(iterator.closed, false, 'iterator remains unclosed');

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
