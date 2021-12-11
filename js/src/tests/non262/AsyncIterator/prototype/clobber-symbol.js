// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))
//
//

/*---
esid: pending
description: %AsyncIterator.prototype% methods work even if the global Symbol has been clobbered.
info: >
  Iterator Helpers proposal 2.1.6
features: [iterator-helpers, Symbol.asyncIterator]
---*/

Symbol = undefined;
assertThrowsInstanceOf(() => Symbol.asyncIterator, TypeError);

async function* gen(value) {
  yield value;
}

const lazyMethods = [
  iter => iter.map(x => x),
  iter => iter.filter(x => x),
  iter => iter.take(1),
  iter => iter.drop(0),
  iter => iter.asIndexedPairs(),
  iter => iter.flatMap(gen),
];

const strictMethods = [
  iter => iter.reduce((_, x) => x, undefined),
  iter => iter.toArray(),
  iter => iter.forEach(() => undefined),
  iter => iter.some(x => true),
  iter => iter.every(x => true),
  iter => iter.find(x => true),
];

(async () => {
  for (const method of lazyMethods) {
    const {value} = await method(gen('value')).next();
    assertEq(Array.isArray(value) ? value[1] : value, 'value');
  }

  for (const method of strictMethods) {
    await method(gen('value'));
  }
})();

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
