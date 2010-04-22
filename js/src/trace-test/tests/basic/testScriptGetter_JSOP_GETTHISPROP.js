var a = {
    get p() { return 11; },
    test: function () {
	var s = 0;
	for (var i = 0; i < 9; i++)
	    s += this.p;
	assertEq(s, 99);
    }};
a.test();

checkStats({recorderStarted: 1, recorderAborted: 0, traceCompleted: 1, traceTriggered: 1});
