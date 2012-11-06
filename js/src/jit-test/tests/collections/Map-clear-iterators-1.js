// A Map iterator does not visit entries removed by clear().

load(libdir + "asserts.js");

var m = Map();
var it = m.iterator();
m.clear();
assertThrowsValue(it.next.bind(it), StopIteration);

m = Map([["a", 1], ["b", 2], ["c", 3], ["d", 4]]);
it = m.iterator();
assertEq(it.next()[0], "a");
m.clear();
assertThrowsValue(it.next.bind(it), StopIteration);

var log = "";
m = Map([["a", 1], ["b", 2], ["c", 3], ["d", 4]]);
for (var [k, v] of m) {
    log += k + v;
    if (k == "b")
        m.clear();
}
assertEq(log, "a1b2");
