// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

//
//
/*---
esid: pending
description: Lazy %Iterator.prototype% method calls can be interleaved.
info: >
  Iterator Helpers proposal 2.1.5
features: [iterator-helpers]
---*/

class TestIterator extends Iterator {
  value = 0;
  next() { 
    return {done: false, value: this.value++};
  }
}

function unwrapResult(result) {
  return result;
}

const methods = [
  iter => iter.map(x => x),
  iter => iter.filter(x => true),
  iter => iter.take(2),
  iter => iter.drop(0),
  iter => iter.flatMap(x => [x]),
];

for (const firstMethod of methods) {
  for (const secondMethod of methods) {
    const iterator = new TestIterator();
    const firstHelper = firstMethod(iterator);
    const secondHelper = secondMethod(iterator);

    let firstResult = unwrapResult(firstHelper.next());
    assertEq(firstResult.done, false);
    assertEq(firstResult.value, 0);

    let secondResult = unwrapResult(secondHelper.next());
    assertEq(secondResult.done, false);
    assertEq(secondResult.value, 1);

    firstResult = unwrapResult(firstHelper.next());
    assertEq(firstResult.done, false);
    assertEq(firstResult.value, 2);

    secondResult = unwrapResult(secondHelper.next());
    assertEq(secondResult.done, false);
    assertEq(secondResult.value, 3);
  }
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
