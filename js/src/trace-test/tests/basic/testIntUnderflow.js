function testIntUnderflow() {
    // int32_min + 8
    var ival = -2147483648 + 8;
    for (var i = 0; i < 30; i++) {
        ival -= 2;
    }
    return (ival > -2147483648);
}
assertEq(testIntUnderflow(), false);
checkStats({
    recorderStarted: 2,
    recorderAborted: 0,
    traceCompleted: 2,
    traceTriggered: 2,
});
