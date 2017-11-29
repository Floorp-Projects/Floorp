var g = newGlobal();
g.eval("azx918 = 1");
for (var x in g) {
    assertEq(x, x);
}
