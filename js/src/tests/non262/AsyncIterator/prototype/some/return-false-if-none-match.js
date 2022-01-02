// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 

async function* gen() {
  yield 1;
  yield 3;
  yield 5;
}
const fn = x => x % 2 == 0;

gen().some(fn).then(result => assertEq(result, false));

async function* empty() {}
empty().some(x => x).then(result => assertEq(result, false));

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
