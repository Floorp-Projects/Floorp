// A Set iterator does not visit entries removed by clear().

load(libdir + "asserts.js");

var s = Set();
var it = s.iterator();
s.clear();
assertThrowsValue(it.next.bind(it), StopIteration);

s = Set(["a", "b", "c", "d"]);
it = s.iterator();
assertEq(it.next()[0], "a");
s.clear();
assertThrowsValue(it.next.bind(it), StopIteration);

var log = "";
s = Set(["a", "b", "c", "d"]);
for (var v of s) {
    log += v;
    if (v == "b")
        s.clear();
}
assertEq(log, "ab");
