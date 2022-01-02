function f(i) {
    if (i == 1500)
        g();
    const x = 42;
    function g() {
        return x;
    }
    return g;
}

var caught = false;
var i;
try {
    for (i = 0; i < 2000; i++)
        assertEq(f(i)(), 42);
} catch(e) {
    assertEq(e instanceof ReferenceError, true);
    assertEq(i, 1500);
    caught = true;
}
assertEq(caught, true);

