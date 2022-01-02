// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 

const reducer = (x, y) => 0;
async function *gen() {}

gen().reduce(reducer, 1).then(result => assertEq(result, 1));

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
