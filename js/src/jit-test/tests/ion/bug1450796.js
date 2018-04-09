function f() {
    var t = new Float32Array(1);
    t[t.length] = 1;
    return t[t.length];
}
for (var i = 0; i < 5; i++)
    assertEq(f(), undefined);
