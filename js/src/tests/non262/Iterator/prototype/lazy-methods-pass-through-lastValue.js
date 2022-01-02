// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

class TestIterator extends Iterator {
  next(value) { 
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
  const iterator = new TestIterator();
  const iteratorHelper = method(iterator);
  iteratorHelper.next();
  let result = iteratorHelper.next('last value');
  assertEq(result.done, false);
  // Array.isArray is used to make sure we unwrap asIndexedPairs results.
  assertEq(Array.isArray(result.value) ? result.value[1] : result.value, 'last value');
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);

