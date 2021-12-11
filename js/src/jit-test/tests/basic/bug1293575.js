
function f(y) {
    y = 123456;
    for (var x = 0; x < 9; ++x) {
        z = arguments.callee.arguments;
        assertEq(z[0], Math);
    }
}
f(Math);
