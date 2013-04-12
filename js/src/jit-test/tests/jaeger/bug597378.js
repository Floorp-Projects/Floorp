// vim: set ts=8 sts=4 et sw=4 tw=99:
function f(a, b) {
    var o = a;
    var q = b;
    var p;
    do { } while (0);
    p = o;
    q = p + 1 < q ? p + 1 : 0;
    assertEq(q, 0);
}
f(3, 4);

