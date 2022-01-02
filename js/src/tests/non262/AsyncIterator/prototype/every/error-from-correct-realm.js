// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 

const otherGlobal = newGlobal({newCompartment: true});
assertEq(TypeError !== otherGlobal.TypeError, true);

async function *gen() {}

gen().every().then(() => assertEq(true, false, 'expected error'), err => {
  assertEq(err instanceof TypeError, true);
});

otherGlobal.AsyncIterator.prototype.every.call(gen()).then(() => assertEq(true, false, 'expected error'), err => {
  assertEq(
    err instanceof otherGlobal.TypeError,
    true,
    'TypeError comes from the realm of the method.',
  );
});

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
