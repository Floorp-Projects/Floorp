// vim: set ts=4 sw=4 tw=99 et:

function f(a) {
    var x = a;
    var y = x;

    assertEq((x << y), (a << a));
    assertEq((y << x), (a << a));
}

f(2);

