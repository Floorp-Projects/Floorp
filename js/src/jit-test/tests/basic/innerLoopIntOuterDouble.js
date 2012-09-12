function innerLoopIntOuterDouble() {
    var n = 1000, i=0, j=0, count=0, limit=0;
    for (i = 1; i <= n; ++i) {
        limit = i * 1;
        for (j = 0; j < limit; ++j) {
            ++count;
        }
    }
    return "" + count;
}
assertEq(innerLoopIntOuterDouble(), "500500");
