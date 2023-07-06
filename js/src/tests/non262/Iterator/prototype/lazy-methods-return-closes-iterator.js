// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

//
//
/*---
esid: pending
description: Calling `.return()` on a lazy %Iterator.prototype% method closes the source iterator.
info: >
  Iterator Helpers proposal 2.1.5
features: [iterator-helpers]
---*/

class TestIterator extends Iterator {
  next() { 
    return {done: false, value: 1};
  }

  closed = false;
  return(value) {
    this.closed = true;
    return {done: true, value};
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
  const iter = new TestIterator();
  const iterHelper = method(iter);
  iterHelper.next();

  assertEq(iter.closed, false);
  const result = iterHelper.return("ignored");
  assertEq(iter.closed, true);
  assertEq(result.done, true);
  assertEq(result.value, undefined);
}

for (const method of methods) {
  const iter = new TestIterator();
  const iterHelper = method(iter);

  assertEq(iter.closed, false);
  const result = iterHelper.return("ignored");
  assertEq(iter.closed, true);
  assertEq(result.done, true);
  assertEq(result.value, undefined);
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
