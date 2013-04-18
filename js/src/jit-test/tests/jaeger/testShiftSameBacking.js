// vim: set ts=8 sts=4 et sw=4 tw=99:

function f(a) {
    var x = a;
    var y = x;

    assertEq((x << y), (a << a));
    assertEq((y << x), (a << a));
}

f(2);

