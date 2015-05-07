function f(x) {
    for (var i = 0; i < 100000; i++ ) {
        [(x, 2)];
        try { g(); } catch (t) {}
    }
}
f(2);
