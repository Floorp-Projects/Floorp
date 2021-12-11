// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

//
//
/*---
esid: pending
description: Lazy %Iterator.prototype% methods throw eagerly when `next` is non-callable.
info: >
  Iterator Helpers proposal 1.1.1
features: [iterator-helpers]
---*/

const methods = [
  next => Iterator.prototype.map.bind({next}, x => x),
  next => Iterator.prototype.filter.bind({next}, x => x),
  next => Iterator.prototype.take.bind({next}, 1),
  next => Iterator.prototype.drop.bind({next}, 0),
  next => Iterator.prototype.asIndexedPairs.bind({next}),
  next => Iterator.prototype.flatMap.bind({next}, x => [x]),
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
