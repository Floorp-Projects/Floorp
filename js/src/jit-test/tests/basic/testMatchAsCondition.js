function testMatchAsCondition() {
    var a = ['0', '0', '0', '0'];
    var r = /0/;
    "x".q;
    for (var z = 0; z < 4; z++)
        a[z].match(r) ? 1 : 2;
}
testMatchAsCondition();
