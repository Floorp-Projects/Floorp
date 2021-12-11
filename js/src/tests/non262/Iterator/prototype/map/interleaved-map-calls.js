// |reftest| skip-if(!this.hasOwnProperty('Iterator'))
//

/*---
esid: pending
description: Interleaved %Iterator.prototype%.map calls on the same iterator.
info:
features: [iterator-helpers]
---*/

const iterator = [1, 2, 3].values();
const mapped1 = iterator.map(x => x);
const mapped2 = iterator.map(x => 0);

assertEq(mapped1.next().value, 1);
assertEq(mapped2.next().value, 0);
assertEq(mapped1.next().value, 3);

assertEq(mapped1.next().done, true);
assertEq(mapped2.next().done, true);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
