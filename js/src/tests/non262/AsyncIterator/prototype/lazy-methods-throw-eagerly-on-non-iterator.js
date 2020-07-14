// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: Lazy %AsyncIterator.prototype% methods throw eagerly when called on non-iterators.
info: >
  AsyncIterator Helpers proposal 1.1.1
features: [iterator-helpers]
---*/

async function* gen(x) { yield x; }

const methods = [
  iter => AsyncIterator.prototype.map.bind(iter, x => x),
  iter => AsyncIterator.prototype.filter.bind(iter, x => x),
  iter => AsyncIterator.prototype.take.bind(iter, 1),
  iter => AsyncIterator.prototype.drop.bind(iter, 0),
  iter => AsyncIterator.prototype.asIndexedPairs.bind(iter),
  iter => AsyncIterator.prototype.flatMap.bind(iter, gen),
];

for (const method of methods) {
  for (const value of [undefined, null, 0, false, '', Symbol(''), {}, []]) {
    assertThrowsInstanceOf(method(value), TypeError);
  }
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
