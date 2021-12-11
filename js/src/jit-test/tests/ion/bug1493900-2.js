function f(a, b) {
    for (; b.x < 0; b = b.x) {
        while (b === b) {};
    }
    for (var i = 0; i < 99999; ++i) {}
}
f(0, 0);
