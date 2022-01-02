function foo() {
    var obj = new Object();
    var index = [-0, 2147483648, 1073741825];
    for (var j in index) {
        testProperty(index[j]);
    }
    function testProperty(i) {
        obj[i] = '' + i;
    }
    assertEq(JSON.stringify(obj), '{"0":"0","1073741825":"1073741825","2147483648":"2147483648"}');
}
foo();
