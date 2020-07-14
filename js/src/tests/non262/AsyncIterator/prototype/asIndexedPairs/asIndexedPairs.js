// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

async function* gen() {
  yield 1;
  yield 2;
  yield 3;
}

let iter = gen().asIndexedPairs();

for (const v of [[0, 1], [1, 2], [2, 3]]) {
  iter.next().then(
    result => {
      console.log(result);
      assertEq(result.done, false);
      assertEq(result.value[0], v[0]);
      assertEq(result.value[1], v[1]);
    }
  );
}

iter.next().then(({done}) => assertEq(done, true));

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
