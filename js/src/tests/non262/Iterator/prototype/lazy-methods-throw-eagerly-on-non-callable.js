// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

//
//
/*---
esid: pending
description: Lazy %Iterator.prototype% methods throw eagerly when passed non-callables.
info: >
  Iterator Helpers proposal 2.1.5
features: [iterator-helpers]
---*/

const methods = [
  (iter, fn) => iter.map(fn),
  (iter, fn) => iter.filter(fn),
  (iter, fn) => iter.flatMap(fn),
];

for (const method of methods) {
  assertThrowsInstanceOf(() => method(Iterator.prototype, 0), TypeError);
  assertThrowsInstanceOf(() => method(Iterator.prototype, false), TypeError);
  assertThrowsInstanceOf(() => method(Iterator.prototype, undefined), TypeError);
  assertThrowsInstanceOf(() => method(Iterator.prototype, null), TypeError);
  assertThrowsInstanceOf(() => method(Iterator.prototype, ''), TypeError);
  assertThrowsInstanceOf(() => method(Iterator.prototype, Symbol('')), TypeError);
  assertThrowsInstanceOf(() => method(Iterator.prototype, {}), TypeError);

  assertThrowsInstanceOf(() => method([].values(), 0), TypeError);
  assertThrowsInstanceOf(() => method([].values(), false), TypeError);
  assertThrowsInstanceOf(() => method([].values(), undefined), TypeError);
  assertThrowsInstanceOf(() => method([].values(), null), TypeError);
  assertThrowsInstanceOf(() => method([].values(), ''), TypeError);
  assertThrowsInstanceOf(() => method([].values(), Symbol('')), TypeError);
  assertThrowsInstanceOf(() => method([].values(), {}), TypeError);
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
