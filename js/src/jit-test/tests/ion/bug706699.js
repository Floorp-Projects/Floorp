// |jit-test| error: 2
function g(x, y) {
    return x + y;
}

function f(g, x, y) {
    // return x + y;
    return g(x, y);
}

assertEq(g(4, 5), 9);

obj = { valueOf: function () { throw 2; } };

print(f(g, obj, 2));

