// A Set iterator does not visit entries removed by clear().

load(libdir + "iteration.js");

var s = new Set();
var it = s[Symbol.iterator]();
s.clear();
assertIteratorDone(it, undefined);

s = new Set(["a", "b", "c", "d"]);
it = s[Symbol.iterator]();
assertIteratorNext(it, "a");
s.clear();
assertIteratorDone(it, undefined);

var log = "";
s = new Set(["a", "b", "c", "d"]);
for (var v of s) {
    log += v;
    if (v == "b")
        s.clear();
}
assertEq(log, "ab");
