function f() {
    var a = [], i, N = 10;
    for (i = 0; i < N; i++)
        a[i] = {m: function() { return 0; }, m: function() { return 1; }};
    assertEq(a[N - 2].m === a[N - 1].m, false);
    for (i = 0; i < N; i++) {
        var f = a[i].m;
        assertEq(f(), 1);
    }
}
f();
f();
