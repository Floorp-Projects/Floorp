// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 

const reducer = (acc, value) => acc + value;
async function* gen() {
  yield 1;
  yield 2;
  yield 3;
}

gen().reduce(reducer, 0).then(result => assertEq(result, 6));

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
