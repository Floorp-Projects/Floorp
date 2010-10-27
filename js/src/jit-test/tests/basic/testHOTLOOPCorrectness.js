function testHOTLOOPCorrectness() {
    var b = 0;
    for (var i = 0; i < HOTLOOP; ++i)
        ++b;
    return b;
}
// Change the global shape right before doing the test
this.testHOTLOOPCorrectnessVar = 1;
assertEq(testHOTLOOPCorrectness(), HOTLOOP);
checkStats({
    recorderStarted: 1,
    recorderAborted: 0,
    traceTriggered: 0
});
