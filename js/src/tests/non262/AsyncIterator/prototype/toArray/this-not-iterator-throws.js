// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 


function check(x) {
  AsyncIterator.prototype.toArray.call(x).then(
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
