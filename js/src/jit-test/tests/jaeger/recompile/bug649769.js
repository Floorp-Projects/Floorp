
function g(x) {
    if (!x) {
        throw 1;
    }
}

function f(a, b, c, d) {
    var x = [].push(3);
    g(true);
    assertEq(x, 1);
}
f(1.2, 2, 3, 4);
gc();
f(1, 2, 3, 4);

