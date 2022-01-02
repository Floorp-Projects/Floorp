// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

async function* gen() {
  yield 1;
  yield 2;
  yield 3;
}

async function* inner(x) {
  yield x;
  yield x + 1;
}

let iter = gen().flatMap(x => inner(x));

for (const v of [1, 2, 2, 3, 3, 4]) {
  iter.next().then(
    ({done, value}) => {
      assertEq(done, false);
      assertEq(value, v);
    }
  );
}

iter.next().then(({done}) => assertEq(done, true));

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
