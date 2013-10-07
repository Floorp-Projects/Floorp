// A map iterator can cope with removing the next entry, then the current entry.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

var map = Map([['a', 0], ['b', 1], ['c', 2], ['d', 3]]);
var iter = map[std_iterator]();
assertIteratorResult(iter.next(), ['a', 0], false);
assertIteratorResult(iter.next(), ['b', 1], false);
map.delete('c');
map.delete('b');
assertIteratorResult(iter.next(), ['d', 3], false);
assertIteratorResult(iter.next(), undefined, true);
