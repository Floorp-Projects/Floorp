function testNativeSetter() {
    var re = /foo/;
    var N = RUNLOOP + 10;
    for (var i = 0; i < N; i++)
        re.lastIndex = i;
    assertEq(re.lastIndex, N - 1);
}
testNativeSetter();
checkStats({
    recorderStarted: 1,
    recorderAborted: 0,
    traceTriggered: 1,
    sideExitIntoInterpreter: 1
});
