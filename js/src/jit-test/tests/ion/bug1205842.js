function f(x) {
    (function() {
        x = 0;
    })();
}
for (var k = 0; k < 9; k++) {
    f(Math.fround(1));
}
