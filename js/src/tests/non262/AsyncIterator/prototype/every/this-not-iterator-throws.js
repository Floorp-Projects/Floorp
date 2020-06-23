// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

const fn = x => x;

function check(x) {
  AsyncIterator.prototype.every.call(x, fn).then(
    () => {
      throw new Error('check should have been rejected');
    },
    err => {
      assertEq(err instanceof TypeError, true);
    }
  );
}

check();
check(undefined);
check({});
check({next: 0});

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
