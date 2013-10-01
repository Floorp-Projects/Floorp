// A closed Set iterator does not visit new entries added after a clear().

load(libdir + "asserts.js");

var s = Set();
var it = s.iterator();
assertThrowsValue(it.next.bind(it), StopIteration);  // close the iterator
s.clear();
s.add("a");
assertThrowsValue(it.next.bind(it), StopIteration);
