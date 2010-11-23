function f() {
    var n;
    var k;
    for (var i = 0; i < 2*RUNLOOP; ++i) {
	n = null;
	k = n++;
	if (k) { }
    }
    return [k, n];
}

var [a, b] = f();
assertEq(a, 0);
assertEq(b, 1);

checkStats({
    recorderStarted: 1,
    recorderAborted: 0,
    traceCompleted: 1,
    traceTriggered: 1
});
