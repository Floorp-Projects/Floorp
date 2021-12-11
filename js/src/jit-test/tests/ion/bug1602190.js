function g(x) {
    var z = true;
    return (x ? 0.1 : (z ? Math.fround(1) : NaN));
}
function f() {
    var arr = [1];
    for (var i = 0; i < 550; ++i) {
        for (var j = 0; j < 2; ++j) {
            assertEq(g(arr[j]), j ? 1 : 0.1);
        }
    }
}
f();
