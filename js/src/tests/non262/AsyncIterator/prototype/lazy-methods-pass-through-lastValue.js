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

for (const method of methods) {
  const iterator = new TestAsyncIterator();
  const iteratorHelper = method(iterator);
  iteratorHelper.next().then(
    _ => iteratorHelper.next('last value').then(
      ({done, value}) => {
        assertEq(done, false);
        // Unwrap the return value from asIndexedPairs.
        assertEq(Array.isArray(value) ? value[1] : value, 'last value');
      }
    ),
  );
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);

