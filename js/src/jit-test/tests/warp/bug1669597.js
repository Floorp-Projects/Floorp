// |jit-test| --fast-warmup
var str = '';
function g(x) {
    with(this) {} // Don't inline.
    return x;
}
function f() {
    var x = 0;
    for (var i = 0; i < 100; i++) {
        x += +g(+str);
    }
    return x;
}
assertEq(f(), 0);
