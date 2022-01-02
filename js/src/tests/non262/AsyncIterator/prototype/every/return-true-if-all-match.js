// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

async function* gen() {
  yield 1;
  yield 3;
  yield 5;
}
const fn = x => x % 2 == 1;

gen().every(fn).then(result => assertEq(result, true));

async function* empty() {}
empty().every(x => x).then(result => assertEq(result, true));

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
