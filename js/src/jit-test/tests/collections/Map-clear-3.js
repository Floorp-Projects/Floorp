// Clearing a Map with a nontrivial number of elements works.

var m = Map();
for (var i = 0; i < 100; i++)
    m.set(i, i);
assertEq(m.size, i);
m.clear();
assertEq(m.size, 0);
m.set("a", 1);
assertEq(m.get("a"), 1);
