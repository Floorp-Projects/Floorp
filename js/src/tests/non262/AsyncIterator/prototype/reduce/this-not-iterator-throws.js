// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 

const sum = (x, y) => x + y;
function check(x) {
  AsyncIterator.prototype.reduce.call(x, sum).then(
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
