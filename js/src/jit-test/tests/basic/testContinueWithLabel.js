function testContinueWithLabel() {
    var i = 0;
    var j = 20;
    checkiandj :
    while (i < 10) {
        i += 1;
        checkj :
        while (j > 10) {
            j -= 1;
            if ((j % 2) == 0)
            continue checkj;
        }
    }
    return i + j;
}
assertEq(testContinueWithLabel(), 20);
