function f() {
    return Object(Symbol());
}

for (var i = 0; i < 4; i++) {
    f();
    gc();
}
