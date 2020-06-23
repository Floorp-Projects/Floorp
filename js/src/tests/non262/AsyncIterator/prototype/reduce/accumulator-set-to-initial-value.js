// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 

const reducer = (acc, _) => acc;
async function* gen() {
  yield 1;
  yield 2;
  yield 3;
}

gen().reduce(reducer, 0).then(value => assertEq(value, 0));

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
