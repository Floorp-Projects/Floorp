function f(a, c) {
    if (c) {
        a++;
    } else {
        a--;
    }
    return (a + a) | 0;
}

with ({}) {}
for (var i = 0; i < 100; i++) {
    f(2147483647, i % 2);
}
