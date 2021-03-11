var dummy;
function foo(a) {
    dummy = arguments.length;
    return a;
}

function bar(x) {
    var arg = x ? 1 : 2;
    assertEq(foo(arg), arg);
}

with({}) {}
for (var i = 0; i < 50; i++) {
    bar(i % 2 == 0);
}
