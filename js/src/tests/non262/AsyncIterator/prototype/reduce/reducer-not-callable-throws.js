// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 

async function *gen() {}

function check(fn) {
  gen().reduce(fn).then(() => {
    throw new Error('every should have thrown');
  },
  err => {
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
check({});

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
