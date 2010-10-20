function testRUNLOOPCorrectness() {
    var b = 0;
    for (var i = 0; i < RUNLOOP; ++i) {
    ++b;
    }
    return b;
}
// Change the global shape right before doing the test
this.testRUNLOOPCorrectnessVar = 1;
assertEq(testRUNLOOPCorrectness(), RUNLOOP);
checkStats({
    recorderStarted: 1,
    recorderAborted: 0,
    traceTriggered: 1
});
