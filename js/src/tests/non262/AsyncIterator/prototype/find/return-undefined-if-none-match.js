// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 

async function* gen() {
  yield 1;
  yield 3;
  yield 5;
}
const fn = x => x % 2 == 0;

gen().find(fn).then(result => assertEq(result, undefined));

async function* empty() {}
empty().find(x => x).then(result => assertEq(result, undefined));

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
