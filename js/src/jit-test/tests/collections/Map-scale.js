// Maps can hold at least 64K values.

var N = 1 << 16;
var m = new Map;
for (var i = 0; i < N; i++)
    assertEq(m.set(i, i), undefined);
for (var i = 0; i < N; i++)
    assertEq(m.get(i), i);
