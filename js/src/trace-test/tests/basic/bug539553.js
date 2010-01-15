function g(x) {
    assertEq(x.length, 1);
}

function f() {
    arguments.length = 1;
    for (var i = 0; i < 9; i++)
        g(arguments);
}

f();
