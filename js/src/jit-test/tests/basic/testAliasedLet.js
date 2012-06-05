function f() {
    let (x, y, z) {
        eval('x = 1; y = 2; z = 3');
        for (var i = 0; i < 10000; ++i) {
            assertEq(x, 1);
            assertEq(y, 2);
            assertEq(z, 3);
        }
    }
}
f();
