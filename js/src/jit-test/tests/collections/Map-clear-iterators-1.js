// A Map iterator does not visit entries removed by clear().

load(libdir + "iteration.js");

var m = Map();
var it = m[std_iterator]();
m.clear();
assertIteratorDone(it, undefined);

m = Map([["a", 1], ["b", 2], ["c", 3], ["d", 4]]);
it = m[std_iterator]();
assertIteratorNext(it, ["a", 1]);
m.clear();
assertIteratorDone(it, undefined);

var log = "";
m = Map([["a", 1], ["b", 2], ["c", 3], ["d", 4]]);
for (var [k, v] of m) {
    log += k + v;
    if (k == "b")
        m.clear();
}
assertEq(log, "a1b2");
