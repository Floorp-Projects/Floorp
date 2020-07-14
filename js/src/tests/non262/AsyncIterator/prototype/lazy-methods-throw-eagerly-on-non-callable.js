// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: Lazy %AsyncIterator.prototype% methods throw eagerly when passed non-callables.
info: >
  Iterator Helpers proposal 2.1.6
features: [iterator-helpers]
---*/

async function* gen() {}

const methods = [
  (iter, fn) => iter.map(fn),
  (iter, fn) => iter.filter(fn),
  (iter, fn) => iter.flatMap(fn),
];

for (const method of methods) {
  assertThrowsInstanceOf(() => method(AsyncIterator.prototype, 0), TypeError);
  assertThrowsInstanceOf(() => method(AsyncIterator.prototype, false), TypeError);
  assertThrowsInstanceOf(() => method(AsyncIterator.prototype, undefined), TypeError);
  assertThrowsInstanceOf(() => method(AsyncIterator.prototype, null), TypeError);
  assertThrowsInstanceOf(() => method(AsyncIterator.prototype, ''), TypeError);
  assertThrowsInstanceOf(() => method(AsyncIterator.prototype, Symbol('')), TypeError);
  assertThrowsInstanceOf(() => method(AsyncIterator.prototype, {}), TypeError);

  assertThrowsInstanceOf(() => method(gen(), 0), TypeError);
  assertThrowsInstanceOf(() => method(gen(), false), TypeError);
  assertThrowsInstanceOf(() => method(gen(), undefined), TypeError);
  assertThrowsInstanceOf(() => method(gen(), null), TypeError);
  assertThrowsInstanceOf(() => method(gen(), ''), TypeError);
  assertThrowsInstanceOf(() => method(gen(), Symbol('')), TypeError);
  assertThrowsInstanceOf(() => method(gen(), {}), TypeError);
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
