function testSwitch() {
    var x = 0;
    var ret = 0;
    for (var i = 0; i < 100; ++i) {
        switch (x) {
            case 0:
                ret += 1;
                break;
            case 1:
                ret += 2;
                break;
            case 2:
                ret += 3;
                break;
            case 3:
                ret += 4;
                break;
            default:
                x = 0;
        }
        x++;
    }
    return ret;
}
assertEq(testSwitch(), 226);
