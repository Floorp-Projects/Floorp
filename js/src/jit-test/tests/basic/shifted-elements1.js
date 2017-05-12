function f() {
    var arr = [];
    var iters = 1500;
    for (var i = 0; i < iters; i++) {
        arr.push(i);
        if (i % 2 === 0)
            assertEq(arr.shift(), i / 2);
    }
    assertEq(arr.length, iters / 2);
    for (var i = iters / 2; i < iters; i++)
        assertEq(arr.shift(), i);
    assertEq(arr.length, 0);
}
f();
