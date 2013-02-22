var m = new Map;
var s = new Set;

var A = [];
for (var i = 0; i < 1024; ++i) {
    var key = {i:i};
    m.set(key, i);
    s.add(key);
    A.push(key);
}
gc();
for (var i in A) {
    var key = A[i];
    assertEq(m.has(key), true);
    assertEq(s.has(key), true);
}
