// A map iterator can cope with removing the next entry, then the current entry.

load(libdir + "asserts.js");

var map = Map([['a', 0], ['b', 1], ['c', 2], ['d', 3]]);
var iter = map.iterator();
assertEq(iter.next()[0], 'a');
assertEq(iter.next()[0], 'b');
map.delete('c');
map.delete('b');
assertEq(iter.next()[0], 'd');
assertThrowsValue(function () { iter.next(); }, StopIteration);
