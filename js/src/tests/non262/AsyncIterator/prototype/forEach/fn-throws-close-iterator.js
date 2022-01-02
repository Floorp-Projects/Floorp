// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 

class TestIterator extends AsyncIterator {
  next() {
    return Promise.resolve({ done: this.closed });
  }

  closed = false;
  return() {
    this.closed = true;
  }
}

const fn = () => { throw new Error(); };
const iter = new TestIterator();

assertEq(iter.closed, false);
iter.forEach(fn).then(() => assertEq(true, false, 'expected error'), err => {
  assertEq(err instanceof Error, true);
  assertEq(iter.closed, true);
});

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
