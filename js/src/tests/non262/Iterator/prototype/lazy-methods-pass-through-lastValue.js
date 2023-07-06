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

for (const method of methods) {
  const iterator = new TestIterator();
  const iteratorHelper = method(iterator);
  iteratorHelper.next();
  let result = iteratorHelper.next('last value');
  assertEq(result.done, false);
  assertEq(result.value, 'next value');
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);

