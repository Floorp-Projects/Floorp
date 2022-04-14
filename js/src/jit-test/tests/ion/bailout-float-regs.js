function f() {
    var res = 0;
    for (var i = 0; i < 2000; i++) {
        var res1 = Math.abs(i - 123.5); // Double
        var res2 = Math.fround(i + 0.5); // Float32
        if (i > 1900) {
            bailout();
        }
        res += res1 + res2;
    }
    assertEq(res, 3767376);
}
f();
