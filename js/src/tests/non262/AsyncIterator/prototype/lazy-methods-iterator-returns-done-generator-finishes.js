// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

class TestIterator extends AsyncIterator {
  async next() {
    return {done: true, value: 'value'};
  }
}

async function* gen(x) { yield x; }

const methods = [
  iter => iter.map(x => x),
  iter => iter.filter(x => true),
  iter => iter.take(1),
  iter => iter.drop(0),
  iter => iter.asIndexedPairs(),
  iter => iter.flatMap(gen),
];

(async () => {
  for (const method of methods) {
    const iterator = method(new TestIterator());
    const {done, value} = await iterator.next();
    assertEq(done, true);
    assertEq(value, undefined);
  }
})();

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
