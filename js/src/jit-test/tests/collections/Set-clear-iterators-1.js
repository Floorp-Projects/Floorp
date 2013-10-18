// A Set iterator does not visit entries removed by clear().

load(libdir + "iteration.js");

var s = Set();
var it = s[std_iterator]();
s.clear();
assertIteratorDone(it, undefined);

s = Set(["a", "b", "c", "d"]);
it = s[std_iterator]();
assertIteratorNext(it, "a");
s.clear();
assertIteratorDone(it, undefined);

var log = "";
s = Set(["a", "b", "c", "d"]);
for (var v of s) {
    log += v;
    if (v == "b")
        s.clear();
}
assertEq(log, "ab");
