// Clearing an empty Map has no effect.

var m = Map();
for (var i = 0; i < 2; i++) {
    m.clear();
    assertEq(m.size, 0);
    assertEq(m.has(undefined), false);
}
