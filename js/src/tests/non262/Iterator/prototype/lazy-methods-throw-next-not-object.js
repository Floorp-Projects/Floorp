// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

//
//
/*---
esid: pending
description: Lazy %Iterator.prototype% methods throw if `next` call returns a non-object.
info: >
  Iterator Helpers proposal 2.1.5
features: [iterator-helpers]
---*/

class TestIterator extends Iterator {
  next(value) {
    return value;
  }

  closed = false;
  return() {
    this.closed = true;
    return {done: true};
  }
}

const methods = [
  iter => iter.map(x => x),
  iter => iter.filter(x => x),
  iter => iter.take(1),
  iter => iter.drop(0),
  iter => iter.flatMap(x => [x]),
];

for (const method of methods) {
  for (const value of [undefined, null, 0, false, '', Symbol('')]) {
    const iterator = new TestIterator();
    assertEq(iterator.closed, false);
    assertThrowsInstanceOf(() => method(iterator).next(value), TypeError);
    assertEq(iterator.closed, false);
  }
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
