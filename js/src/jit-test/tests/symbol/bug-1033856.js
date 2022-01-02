function f(x, y) {
    return x == y;
}
f(1.1, 2.2);
for (var i=0; i<5; i++)
    f(1, Symbol());
