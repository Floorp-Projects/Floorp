// A Map iterator does not visit entries removed by clear().

load(libdir + "iteration.js");

var m = Map();
var it = m[std_iterator]();
m.clear();
assertIteratorResult(it.next(), undefined, true);

m = Map([["a", 1], ["b", 2], ["c", 3], ["d", 4]]);
it = m[std_iterator]();
assertIteratorResult(it.next(), ["a", 1], false);
m.clear();
assertIteratorResult(it.next(), undefined, true);

var log = "";
m = Map([["a", 1], ["b", 2], ["c", 3], ["d", 4]]);
for (var [k, v] of m) {
    log += k + v;
    if (k == "b")
        m.clear();
}
assertEq(log, "a1b2");
