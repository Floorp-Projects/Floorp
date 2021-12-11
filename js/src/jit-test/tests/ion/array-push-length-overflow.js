function maybeSetLength(arr, b) {
    with(this) {};
    if (b)
        arr.length = 0x7fffffff;
}
function test() {
    var arr = [];
    for (var i=0; i<2000; i++) {
        maybeSetLength(arr, i > 1500);
        var res = arr.push(2);
        assertEq(res, i > 1500 ? 0x80000000 : i + 1);
    }
}
test();
