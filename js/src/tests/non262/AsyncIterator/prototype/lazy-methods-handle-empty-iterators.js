// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: Lazy %AsyncIterator.prototype% methods handle empty iterators.
info: >
  Iterator Helpers proposal 2.1.6
features: [iterator-helpers]
---*/

class EmptyIterator extends AsyncIterator {
  async next() { 
    return {done: true};
  }
}

async function* gen() {}

const methods = [
  iter => iter.map(x => x),
  iter => iter.filter(x => x),
  iter => iter.take(1),
  iter => iter.drop(0),
  iter => iter.asIndexedPairs(),
  iter => iter.flatMap(x => gen()),
];

for (const method of methods) {
  for (const iterator of [new EmptyIterator(), gen()]) {
    method(iterator).next().then(
      ({done, value}) => {
        assertEq(done, true);
        assertEq(value, undefined);
      }
    );
  }
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
