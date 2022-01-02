// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

const log = [];
async function* gen(n) {
  log.push(`${n}`);
  yield 1;
  log.push(`${n}`);
  yield 2;
}

Promise.all([gen(1).some(() => {}), gen(2).some(() => {})]).then(
  () => {
    assertEq(
      log.join(' '),
      '1 2 1 2',
    );
  },
  err => {
    throw err;
  }
);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
