var a = {_val: 'q',
	 get p() { return f; }};

function f() { return this._val; }

var g = '';
for (var i = 0; i < 9; i++)
    g += a.p();
assertEq(g, 'qqqqqqqqq');

checkStats({recorderStarted: 1, recorderAborted: 0, traceCompleted: 1, traceTriggered: 1});
