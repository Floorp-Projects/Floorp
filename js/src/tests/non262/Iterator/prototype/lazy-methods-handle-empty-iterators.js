// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

//
//
/*---
esid: pending
description: Lazy %Iterator.prototype% methods handle empty iterators.
info: >
  Iterator Helpers proposal 2.1.5
features: [iterator-helpers]
---*/

class EmptyIterator extends Iterator {
  next() { 
    return {done: true};
  }
}

const emptyIterator1 = new EmptyIterator();
const emptyIterator2 = [].values();

const methods = [
  iter => iter.map(x => x),
  iter => iter.filter(x => x),
  iter => iter.take(1),
  iter => iter.drop(0),
  iter => iter.flatMap(x => [x]),
];

for (const method of methods) {
  for (const iterator of [emptyIterator1, emptyIterator2]) {
    const result = method(iterator).next();
    assertEq(result.done, true);
    assertEq(result.value, undefined);
  }
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
