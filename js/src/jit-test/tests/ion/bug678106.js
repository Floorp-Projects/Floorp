function f_localinc(x) {
    var a = x;
    var b = a++;

    var c = b+b+b+b+b+b+b+b+b+b;
    return a + c;
}
assertEq(f_localinc(1), 12)
function f_localdec(x) {
    var a = x;
    var b = a--;

    var c = b+b+b+b+b+b+b+b+b+b;
    return a + c;
}
assertEq(f_localdec(1), 10)
function f_inclocal(x) {
    var a = x;
    var b = ++a;

    var c = b+b+b+b+b+b+b+b+b+b;
    return a + c;
}
assertEq(f_inclocal(1), 22)
function f_declocal(x) {
    var a = x;
    var b = --a;

    var c = b+b+b+b+b+b+b+b+b+b;
    return a + c;
}
assertEq(f_declocal(1), 0)
