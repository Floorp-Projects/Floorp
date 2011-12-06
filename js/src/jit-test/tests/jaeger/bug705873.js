a = []
function f(o) {
    o[5] = {}
}
for (var i = 0; i < 20; i++) {
    with(a) f(a)
}
