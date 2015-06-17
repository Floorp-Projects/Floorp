function f(y) {
    Math.min(NaN) ? a : y
}
function g(y) {
    f({
        e: false
    })
}
x = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
for (var j = 0; j < 23; ++j) {
    g(x[j])
}
