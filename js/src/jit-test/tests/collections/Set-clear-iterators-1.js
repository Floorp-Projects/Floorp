// A Set iterator does not visit entries removed by clear().

load(libdir + "iteration.js");

var s = Set();
var it = s[std_iterator]();
s.clear();
assertIteratorResult(it.next(), undefined, true);

s = Set(["a", "b", "c", "d"]);
it = s[std_iterator]();
assertIteratorResult(it.next(), "a", false);
s.clear();
assertIteratorResult(it.next(), undefined, true);

var log = "";
s = Set(["a", "b", "c", "d"]);
for (var v of s) {
    log += v;
    if (v == "b")
        s.clear();
}
assertEq(log, "ab");
