// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

async function* gen() {
  yield 1;
  yield 2;
  yield 3;
}

let iter = gen().drop(1);

for (const v of [2, 3]) {
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
