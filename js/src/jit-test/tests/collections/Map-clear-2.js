// Clearing a Map removes its entries; the Map remains usable afterwards.

var m = Map([["a", "b"], ["b", "c"]]);
assertEq(m.size, 2);
m.clear();
assertEq(m.size, 0);
assertEq(m.has("a"), false);
assertEq(m.get("a"), undefined);
assertEq(m.delete("a"), false);
assertEq(m.has("b"), false);
for (var pair of m)
    throw "FAIL";  // shouldn't be any pairs

m.set("c", "d");
assertEq(m.size, 1);
assertEq(m.has("a"), false);
assertEq(m.has("b"), false);
