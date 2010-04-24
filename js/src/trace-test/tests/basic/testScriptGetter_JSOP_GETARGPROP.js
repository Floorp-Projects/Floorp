function test(a) {
    var s = '';
    for (var i = 0; i < 9; i++)
	s += a.p;
    assertEq(s, 'qqqqqqqqq');
}
test({get p() { return 'q'; }});

checkStats({recorderStarted: 1, recorderAborted: 0, traceCompleted: 1, traceTriggered: 1});
