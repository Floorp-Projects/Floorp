function g(x,y) {
    return x + y;
}

function f(x) {
    return g.apply(null, arguments);
}

for (var i = 0; i < 100; ++i)
    assertEq(f(i, 1), i+1);
