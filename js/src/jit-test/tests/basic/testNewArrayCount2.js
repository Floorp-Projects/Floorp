function testNewArrayCount2() {
    function count(a) { var n = 0; for (var p in a) n++; return n; }
    var x = 0;
    for (var i = 0; i < 10; ++i)
        x = count(new Array(1,2,3));
    return x;
}
assertEq(testNewArrayCount2(), 3);
