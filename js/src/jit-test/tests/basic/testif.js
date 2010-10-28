function testif() {
    var q = 0;
    for (var i = 0; i < 100; i++) {
        if ((i & 1) == 0)
            q++;
        else
            q--;
    }
    return q;
}
assertEq(testif(), 0);
