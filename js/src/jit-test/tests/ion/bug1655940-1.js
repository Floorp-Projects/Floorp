function f(t) {
    for (var i = 0; i < 2; i++) {
        try {
            var x = 1;
            new Int32Array(1);
            x = 2;
            new Int32Array(t);
        } catch (e) {
            assertEq(x, 2);
        }
    }
}
f(1);
f(-1);
