// Clearing a Set after deleting some entries works.

var s = Set(["a", "b", "c", "d"]);
for (var v of s)
    if (v !== "c")
        s.delete(v);
s.clear();
assertEq(s.size, 0);
assertEq(s.has("c"), false);
assertEq(s.has("d"), false);
