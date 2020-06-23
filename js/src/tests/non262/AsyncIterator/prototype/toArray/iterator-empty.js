// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 

async function* gen() {}
gen().toArray().then(array => {
  assertEq(Array.isArray(array), true);
  assertEq(array.length, 0);
});

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
