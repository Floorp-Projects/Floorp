function testLogicalNotNaN() {
    var i = 0;
    var a = new Array(5);
    while (i < a.length)
        a[i++] = !NaN;
    return a.join();
}
assertEq(testLogicalNotNaN(), "true,true,true,true,true");
