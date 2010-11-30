function f() {
    var n;
    var k;
    for (var i = 0; i < 2*RUNLOOP; ++i) {
	n = undefined;
	k = n++;
	if (k) { }
    }
    return [k, n];
}

var [a, b] = f();

assertEq(isNaN(a), true);
assertEq(isNaN(b), true);

checkStats({
    recorderStarted: 1,
    recorderAborted: 0,
    traceCompleted: 1,
    traceTriggered: 1
});
