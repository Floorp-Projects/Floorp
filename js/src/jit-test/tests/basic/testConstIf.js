function testConstIf() {
    var x;
    for (var j=0;j<5;++j) { if (1.1 || 5) { } x = 2;}
    return x;
}
assertEq(testConstIf(), 2);
