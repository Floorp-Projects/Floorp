function f(x) {
    var a = x;
    return a / 10;
}
for (var i=0; i<100; i++)
    assertEq(f(i * 10), i);

function g(x) {
    var y = x + 1;
    return y / y;
}
for (var i=0; i<100; i++)
    assertEq(g(i), 1);
