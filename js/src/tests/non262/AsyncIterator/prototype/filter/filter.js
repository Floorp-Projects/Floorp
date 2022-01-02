// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

async function* gen() {
  yield 1;
  yield 2;
  yield 3;
}

let iter = gen().filter(x => x % 2);

for (const v of [1, 3]) {
  console.log(iter);
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
