// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

class TestIterator extends Iterator {
  next(value = "next value") {
    assertEq(arguments.length, 0);
    return {done: false, value};
  }
}

const methods = [
  iter => iter.map(x => x),
  iter => iter.filter(x => true),
  iter => iter.take(2),
  iter => iter.drop(0),
];

for (const outerMethod of methods) {
  for (const innerMethod of methods) {
    const iterator = new TestIterator();
    const iteratorChain = outerMethod(innerMethod(iterator));
    iteratorChain.next();
    let result = iteratorChain.next('last value');
    assertEq(result.done, false);
    assertEq(result.value, 'next value');
  }
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
