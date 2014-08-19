function f(x) {
    x((x | 0) + x);
};
try {
    f(1);
} catch (e) {}
for (var k = 0; k < 1; ++k) {
    try {
        f(Symbol());
    } catch (e) {}
}
