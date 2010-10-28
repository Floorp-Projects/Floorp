function testStrict() {
    var n = 10, a = [];
    for (var i = 0; i < 10; ++i) {
        a[0] = (n === 10);
        a[1] = (n !== 10);
        a[2] = (n === null);
        a[3] = (n == null);
    }
    return a.join(",");
}
assertEq(testStrict(), "true,false,false,false");
