// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 

async function* gen() {
  yield 1;
  yield 2;
  yield 3;
}

const log = [];
const fn = (value) => {
  log.push(value.toString());
  return value % 2 == 0;
};

gen().find(fn).then(result => {
  assertEq(result, 2);
  assertEq(log.join(','), '1,2');
});

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
