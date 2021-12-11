// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
//
//

/*---
esid: pending
description: %Iterator.prototype%.map works even if the global Symbol has been clobbered..
info: 
features: [iterator-helpers, Symbol, Symbol.iterator]
---*/

Symbol = undefined;
assertThrowsInstanceOf(() => Symbol.iterator, TypeError);

const iterator = [0].values();
assertEq(
  iterator.map(x => x + 1).next().value, 1,
  '`%Iterator.prototype%.map` still works after Symbol has been clobbered'
);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
