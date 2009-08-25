function testIntOverflow() {
    // int32_max - 7
    var ival = 2147483647 - 7;
    for (var i = 0; i < 30; i++) {
        ival += 2;
    }
    return (ival < 2147483647);
}
assertEq(testIntOverflow(), false);
checkStats({
    recorderStarted: 2,
    recorderAborted: 0,
    traceCompleted: 2,
    traceTriggered: 2,
});
