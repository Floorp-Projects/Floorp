// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

class TestIterator extends Iterator {
  next() {
    return {done: true, value: 'value'};
  }
}

const methods = [
  iter => iter.map(x => x),
  iter => iter.filter(x => true),
  iter => iter.take(1),
  iter => iter.drop(0),
  iter => iter.flatMap(x => [x]),
];

for (const method of methods) {
  const iterator = method(new TestIterator());
  const result = iterator.next();
  assertEq(result.done, true);
  assertEq(result.value, undefined);
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);

