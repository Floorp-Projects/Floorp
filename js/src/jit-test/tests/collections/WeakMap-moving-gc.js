var wm = new WeakMap;
var A = [];
for (var i = 0; i < 1024; ++i) {
    var key = {i:i};
    wm.set(key, i);
    A.push(key);
}
gc();
for (var i in A) {
    var key = A[i];
    assertEq(wm.has(key), true);
}
