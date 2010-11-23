function f() {
    for (var i = 0; i < 2*RUNLOOP; ++i) {
	var n = undefined;
	if (n++) { }
    }
}

f();

checkStats({
    recorderStarted: 1,
    recorderAborted: 0,
    traceCompleted: 1,
    traceTriggered: 1
});
