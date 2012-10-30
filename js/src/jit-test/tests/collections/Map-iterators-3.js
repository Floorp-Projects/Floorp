// A closed Map iterator does not visit new entries added after a clear().

load(libdir + "asserts.js");

var m = Map();
var it = m.iterator();
assertThrowsValue(it.next.bind(it), StopIteration);  // close the iterator
m.clear();
m.set("a", 1);
assertThrowsValue(it.next.bind(it), StopIteration);
