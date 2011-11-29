function f() {
    function g() {
        for (var i = 0; i < 3; i++)
        x = i;
    };
    var [x] = 0;
    g();
    assertEq(x, 2);
    print(x);
}
f();
