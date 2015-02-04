// Clearing a Map after deleting some entries works.

var m = new Map([["a", 1], ["b", 2], ["c", 3], ["d", 4]]);
for (var [k, v] of m)
    if (k !== "c")
        m.delete(k);
m.clear();
assertEq(m.size, 0);
assertEq(m.has("c"), false);
assertEq(m.has("d"), false);
