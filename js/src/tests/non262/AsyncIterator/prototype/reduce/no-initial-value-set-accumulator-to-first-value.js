// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 

const reducer = (acc, _) => acc;
async function* gen() {
  yield 1;
  yield 2;
  yield 3;
}

gen().reduce(reducer).then(result => assertEq(result, 1));

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
