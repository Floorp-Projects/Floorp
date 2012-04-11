function f1(b) {
    var w = 3;
    if (b)
        function w() {}
    return w;
}
assertEq(typeof f1(true), "function");
assertEq(f1(false), 3);

function f2(b, w) {
    if (b)
        function w() {}
    return w;
}
assertEq(typeof f2(true, 3), "function");
assertEq(f2(false, 3), 3);

function f3(b) {
    let (w = 3) {
        if (b)
            function w() {}
        return w;
    }
}
assertEq(f3(true, 3), 3);
assertEq(f3(false), 3);
