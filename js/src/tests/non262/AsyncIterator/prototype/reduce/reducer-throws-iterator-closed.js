// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 

class TestIterator extends AsyncIterator {
  next() {
    return Promise.resolve({ done: this.closed, value: undefined });
  }

  closed = false;
  return() {
    this.closed = true;
  }
}

const reducer = (x, y) => { throw new Error(); };
const iter = new TestIterator();

assertEq(iter.closed, false);
iter.reduce(reducer).then(() => assertEq(true, false, 'expected error'), err => {
  assertEq(err instanceof Error, true);
  assertEq(iter.closed, true);
});

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
