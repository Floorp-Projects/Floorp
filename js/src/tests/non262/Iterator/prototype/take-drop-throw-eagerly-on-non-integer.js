// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

//
//
/*---
esid: pending
description: `take` and `drop` throw eagerly when passed values that can't be converted to numbers.
info: >
  Iterator Helpers proposal 2.1.5.4 and 2.1.5.5
features: [iterator-helpers]
---*/

const iter = [].values();
const methods = [
  value => iter.take(value),
  value => iter.drop(value),
];

const objectWithToPrimitive = {
  [Symbol.toPrimitive]() {
    return {};
  }
};

for (const method of methods) {
  assertThrowsInstanceOf(() => method(0n), TypeError);
  assertThrowsInstanceOf(() => method(Symbol('')), TypeError);
  assertThrowsInstanceOf(() => method(objectWithToPrimitive), TypeError);
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
