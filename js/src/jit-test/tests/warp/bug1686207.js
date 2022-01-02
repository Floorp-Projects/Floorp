function f(x, y) {
    x >> (y >>> 0)
}

with ({}) {}

f(-1, -1)
f(1.5, 0)
for (var i = 0; i < 100; i++) {
    f(0, 0);
}
