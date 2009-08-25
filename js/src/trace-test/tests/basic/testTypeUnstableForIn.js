function testTypeUnstableForIn() {
    var a = [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16];
    var x = 0;
    for (var i in a) {
        i = parseInt(i);
        x++;
    }
    return x;
}
assertEq(testTypeUnstableForIn(), 16);
