// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 

async function* gen() {}
gen().reduce((x, y) => x + y).then(() => assertEq(true, false, 'expected error'), err => {
  assertEq(err instanceof TypeError, true);
});

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
