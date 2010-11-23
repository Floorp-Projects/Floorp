var a, b;

function f(str) {
    var n;
    var k;
    for (var i = 0; i < 2*RUNLOOP; ++i) {
	n = str;
	k = n++;
	if (k) { }
    }
    return [k, n];
}

[a, b] = f("10");
assertEq(a, 10);
assertEq(b, 11);

[a, b] = f("5");
assertEq(a, 5);
assertEq(b, 6);

checkStats({
    recorderStarted: 1,
    recorderAborted: 0,
    traceCompleted: 1,
    traceTriggered: 2
});
