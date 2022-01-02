// A map iterator can cope with removing the next entry.

load(libdir + "iteration.js");

var set = new Set("abcd");
var iter = set[Symbol.iterator]();
var log = "";
for (let x of iter) {
    log += x;
    if (x === "b")
        set.delete("c");
}
assertEq(log, "abd");
