

function f(x, y) {
    return Math.imul(0, Math.imul(y | 0, x >> 0))
}
for (var i = 0; i < 2; i++) {
    try {
        (f(1 ? 0 : undefined))()
    } catch (e) {}
}
