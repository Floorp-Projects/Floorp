// A map iterator can cope with removing the next entry, then the current entry.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

var map = new Map([['a', 0], ['b', 1], ['c', 2], ['d', 3]]);
var iter = map[Symbol.iterator]();
assertIteratorNext(iter, ['a', 0]);
assertIteratorNext(iter, ['b', 1]);
map.delete('c');
map.delete('b');
assertIteratorNext(iter, ['d', 3]);
assertIteratorDone(iter, undefined);
