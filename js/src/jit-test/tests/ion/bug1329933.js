
function g(f) {
    for (var j = 0; j < 999; ++j) {
        f(0 / 0);
    }
}
function h(x) {
    x < 1 ? 0 : Math.imul(x || 0);
}
g(h);
