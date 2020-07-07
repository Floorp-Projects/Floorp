// |reftest| skip-if(!this.hasOwnProperty('Iterator'))
//

/*---
esid: pending
description: %Iterator.prototype%.map outputs correct value at end of iterator.
info:
features: [iterator-helpers]
---*/

const iterator = [0].values().map(x => x);

const iterRecord = iterator.next();
assertEq(iterRecord.done, false);
assertEq(iterRecord.value, 0);

let endRecord = iterator.next();
assertEq(endRecord.done, true);
assertEq(endRecord.value, undefined);

endRecord = iterator.next();
assertEq(endRecord.done, true);
assertEq(endRecord.value, undefined);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
