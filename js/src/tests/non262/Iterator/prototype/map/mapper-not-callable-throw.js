// |reftest| skip-if(!this.hasOwnProperty('Iterator'))
//

/*---
esid: pending
description: Eagerly throw TypeError when `mapper` is not callable.
info:
features: [iterator-helpers]
---*/

assertThrowsInstanceOf(() => Iterator.prototype.map(undefined), TypeError);
assertThrowsInstanceOf(() => [].values().map(undefined), TypeError);

assertThrowsInstanceOf(() => Iterator.prototype.map(null), TypeError);
assertThrowsInstanceOf(() => [].values().map(null), TypeError);

assertThrowsInstanceOf(() => Iterator.prototype.map(0), TypeError);
assertThrowsInstanceOf(() => [].values().map(0), TypeError);

assertThrowsInstanceOf(() => Iterator.prototype.map(false), TypeError);
assertThrowsInstanceOf(() => [].values().map(false), TypeError);

assertThrowsInstanceOf(() => Iterator.prototype.map({}), TypeError);
assertThrowsInstanceOf(() => [].values().map({}), TypeError);

assertThrowsInstanceOf(() => Iterator.prototype.map(''), TypeError);
assertThrowsInstanceOf(() => [].values().map(''), TypeError);

assertThrowsInstanceOf(() => Iterator.prototype.map(Symbol('')), TypeError);
assertThrowsInstanceOf(() => [].values().map(Symbol('')), TypeError);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
