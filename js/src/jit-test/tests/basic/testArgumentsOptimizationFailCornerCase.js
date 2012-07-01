Object.prototype.blah = 42;

function g(a,b,c) {
    assertEq(a, 1);
    assertEq(b, 2);
    assertEq(c, 3);
    return 43;
}

function f() {
    var a = arguments;
    var b = a;
    var s = "blah";
    assertEq(a[s], 42);
    assertEq(b[s], 42);
    assertEq(a[s], 42);
    assertEq(b.length, 3);
    assertEq(a.length, 3);
    assertEq(g.apply(null, b), 43);
}

for (var i = 0; i < 10; ++i)
    f(1,2,3);
