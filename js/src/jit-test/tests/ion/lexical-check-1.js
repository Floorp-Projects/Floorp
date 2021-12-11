function f() {
    const x = 42;
    function g() {
        var s = 0;
        for (var i = 100; i--;)
            s += x;
        return s;
    }
    return g;
}

var func = f();
for (var i = 200; i--;)
    assertEq(func(), 4200);

