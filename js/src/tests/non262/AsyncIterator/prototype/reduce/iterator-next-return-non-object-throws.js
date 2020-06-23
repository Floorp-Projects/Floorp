// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 

class TestIterator extends AsyncIterator {
  constructor(value) {
    super();
    this.value = value;
  }

  next() {
    return Promise.resolve(this.value);
  }
}

const sum = (x, y) => x + y;
function check(value) {
  const iter = new TestIterator(value);
  iter.reduce(sum).then(() => assertEq(true, false, 'expected error'), err => {
    assertEq(err instanceof TypeError, true);
  });
}

check();
check(undefined);
check(null);
check(0);
check(false);
check('');
check(Symbol(''));

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
