// Clearing an empty Set has no effect.

var s = Set();
for (var i = 0; i < 2; i++) {
    s.clear();
    assertEq(s.size, 0);
    assertEq(s.has(undefined), false);
}
