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

for (const outerMethod of methods) {
  for (const innerMethod of methods) {
    const iterator = new TestIterator();
    const iteratorChain = outerMethod(innerMethod(iterator));
    iteratorChain.next();
    let result = iteratorChain.next('last value');
    assertEq(result.done, false);
    // Unwrap the asIndexedPair return values.
    while (Array.isArray(result.value)) {
      result.value = result.value[1];
    }
    assertEq(result.value, 'last value');
  }
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
