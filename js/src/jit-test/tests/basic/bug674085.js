function f(x) {
    for (var i = 0; i < 50; i++) {};
    [1][arguments[0]]++;
    x = 1.2;
}
f(0);
