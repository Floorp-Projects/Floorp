// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

class TestAsyncIterator extends AsyncIterator {
  async next(value) {
    return {done: false, value};
  }
}

const methods = [
  iter => iter.map(x => x),
  iter => iter.filter(x => true),
  iter => iter.take(2),
  iter => iter.drop(0),
  iter => iter.asIndexedPairs(),
];

for (const outerMethod of methods) {
  for (const innerMethod of methods) {
    const iterator = new TestAsyncIterator();
    const iteratorChain = outerMethod(innerMethod(iterator));
    iteratorChain.next().then(
      _ => iteratorChain.next('last value').then(
        ({done, value}) => {
          assertEq(done, false);
          // Unwrap the asIndexedPair return values.
          while (Array.isArray(value)) {
            value = value[1];
          }
          assertEq(value, 'last value');
        }
      ),
    );
  }
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);

