// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 

async function* gen() {
  yield 1;
  yield 2;
  yield 3;
}
assertEq(Array.isArray(gen()), false);

gen().toArray().then(array => {
  assertEq(Array.isArray(array), true);
  assertEq(array.length, 3);

  const expected = [1, 2, 3];
  for (const item of array) {
    const expect = expected.shift();
    assertEq(item, expect);
  }
});

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
