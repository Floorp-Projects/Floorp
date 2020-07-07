// |reftest| skip-if(!this.hasOwnProperty('Iterator'))
//

/*---
esid: pending
description: Mutate an iterator after it has been mapped and returned done.
info:
features: [iterator-helpers]
---*/

const array = [1, 2, 3];
const iterator = [1, 2, 3].values().map(x => x * 2);

assertEq(iterator.next().value, 2);
assertEq(iterator.next().value, 4);
assertEq(iterator.next().value, 6);
assertEq(iterator.next().done, true);

array.push(4);
assertEq(iterator.next().done, true);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
