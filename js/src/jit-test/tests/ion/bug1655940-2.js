function f(t) {
    for (var i = 0; i < 2; i++) {
        try {
            var x = 1;
            var {} = {};
            x = 2;
            var {} = t;
        } catch (e) {
            assertEq(x, 2);
        }
    }
}
for (var t of [{}, null]) {
    f(t);
}
