function f() {
    for (var i = 0; i < 250; i++) {
        o = {};
        for (var j = 0; j < 38; j++) {
            o[i + "-" + j] = 1;
        }
    }
}
gczeal(4);
f();
