function m(f) {
    for (var k = 0; k < 2; ++k) {
        try {
            f()
        } catch (e) {}
    }
}
function g(i) {
    x
}
m(g)
function h() {
    g(Math.sqrt(+((function() {}) < 1)))
}
m(h)
