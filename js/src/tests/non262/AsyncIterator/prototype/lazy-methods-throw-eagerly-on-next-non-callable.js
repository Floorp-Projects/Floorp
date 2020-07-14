// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: Lazy %AsyncIterator.prototype% methods throw eagerly when `next` is non-callable.
info: >
  Iterator Helpers proposal 1.1.1
features: [iterator-helpers]
---*/

async function* gen(x) { yield x; }
const methods = [
  next => AsyncIterator.prototype.map.bind({next}, x => x),
  next => AsyncIterator.prototype.filter.bind({next}, x => x),
  next => AsyncIterator.prototype.take.bind({next}, 1),
  next => AsyncIterator.prototype.drop.bind({next}, 0),
  next => AsyncIterator.prototype.asIndexedPairs.bind({next}),
  next => AsyncIterator.prototype.flatMap.bind({next}, gen),
];

for (const method of methods) {
  assertThrowsInstanceOf(method(0), TypeError);
  assertThrowsInstanceOf(method(false), TypeError);
  assertThrowsInstanceOf(method(undefined), TypeError);
  assertThrowsInstanceOf(method(null), TypeError);
  assertThrowsInstanceOf(method(''), TypeError);
  assertThrowsInstanceOf(method(Symbol('')), TypeError);
  assertThrowsInstanceOf(method({}), TypeError);
  assertThrowsInstanceOf(method([]), TypeError);
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
