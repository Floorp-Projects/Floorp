// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

/*---
esid: pending
description: Call next on an iterator that is being iterated over.
info:
features: [iterator-helpers]
---*/

const iterator = [1, 2, 3].values()
const items = [];

for (const item of iterator.map(x => x ** 2)) {
  const nextItem = iterator.next();
  items.push(item, nextItem.value);
}

assertEq(items[0], 1);
assertEq(items[1], 2);
assertEq(items[2], 9);
assertEq(items[3], undefined);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
