function f_arginc(a) {
    var b = a++;

    var c = b+b+b+b+b+b+b+b+b+b;
    return a + c;
}
assertEq(f_arginc(1), 12)
function f_argdec(a) {
    var b = a--;

    var c = b+b+b+b+b+b+b+b+b+b;
    return a + c;
}
assertEq(f_argdec(1), 10)
function f_incarg(a) {
    var b = ++a;

    var c = b+b+b+b+b+b+b+b+b+b;
    return a + c;
}
assertEq(f_incarg(1), 22)
function f_decarg(a) {
    var b = --a;

    var c = b+b+b+b+b+b+b+b+b+b;
    return a + c;
}
assertEq(f_decarg(1), 0)
