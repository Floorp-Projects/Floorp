function f() {
    if (typeof Symbol === "function")
        return Object(Symbol());
}

for (var i = 0; i < 4; i++) {
    f();
    gc();
}
