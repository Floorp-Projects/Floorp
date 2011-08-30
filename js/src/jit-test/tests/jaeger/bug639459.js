function f() {
    var a = [].length;
    return a / a;
}
assertEq(f(), NaN);

