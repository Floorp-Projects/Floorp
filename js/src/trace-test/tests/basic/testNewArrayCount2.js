function testNewArrayCount2() {
    var x = 0;
    for (var i = 0; i < 10; ++i)
        x = new Array(1,2,3).__count__;
    return x;
}
assertEq(testNewArrayCount2(), 3);
