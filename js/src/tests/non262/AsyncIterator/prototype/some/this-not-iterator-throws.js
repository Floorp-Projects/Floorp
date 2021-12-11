// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 

const fn = x => x;
function check(x) {
  AsyncIterator.prototype.some.call(x, fn).then(
    () => assertEq(true, false, 'expected error'),
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
