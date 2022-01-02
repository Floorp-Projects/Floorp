// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

async function* gen(values) {
  yield* values;
}

const otherAsyncIteratorProto = newGlobal({newCompartment: true}).AsyncIterator.prototype;

const methods = [
  ["map", x => x],
  ["filter", x => true],
  ["take", Infinity],
  ["drop", 0],
  ["asIndexedPairs", undefined],
  ["flatMap", x => gen([x])],
];

(async () => {
  for (const [method, arg] of methods) {
    const iterator = gen([1, 2, 3]);
    const helper = otherAsyncIteratorProto[method].call(iterator, arg);

    for (const expected of [1, 2, 3]) {
      const {done, value} = await helper.next();
      assertEq(done, false);
      assertEq(Array.isArray(value) ? value[1] : value, expected);
    }

    const {done, value} = await helper.next();
    assertEq(done, true);
    assertEq(value, undefined);
  }
})();

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
