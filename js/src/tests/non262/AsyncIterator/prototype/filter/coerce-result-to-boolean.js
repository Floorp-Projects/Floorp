// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

async function* gen(iterable) {
  yield* iterable;
}

// All truthy values are kept.
const truthyValues = [true, 1, [], {}, 'test'];
(async () => {
  for await (const value of gen([...truthyValues]).filter(x => x)) {
    assertEq(truthyValues.shift(), value);
  }
})();

// All falsy values are filtered out.
const falsyValues = [false, 0, '', null, undefined, NaN, -0, 0n, createIsHTMLDDA()];
gen(falsyValues).filter(x => x).next().then(
  ({done, value}) => {
    assertEq(done, true);
    assertEq(value, undefined);
  }
);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
