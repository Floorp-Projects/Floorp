// vim: set ts=4 sw=4 tw=99 et:
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

