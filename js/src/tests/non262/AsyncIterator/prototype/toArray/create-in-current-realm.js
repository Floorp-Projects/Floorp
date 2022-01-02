// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 

const otherGlobal = newGlobal({newCompartment: true});

async function* gen() {
  yield 1;
  yield 2;
  yield 3;
}

gen().toArray().then(array => {
  assertEq(array instanceof Array, true);
  assertEq(array instanceof otherGlobal.Array, false);
});

otherGlobal.AsyncIterator.prototype.toArray.call(gen()).then(array => {
  assertEq(array instanceof Array, false);
  assertEq(array instanceof otherGlobal.Array, true);
});

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
