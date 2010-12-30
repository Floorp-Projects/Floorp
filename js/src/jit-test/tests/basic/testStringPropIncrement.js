function f() {
    var o = { n: "" };
    for (var i = 0; i < 2*RUNLOOP; ++i) {
	o.n = "";
	if (o.n++) { }
    }
}

f();

checkStats({
    recorderStarted: 1,
    recorderAborted: 0,
    traceCompleted: 1,
    traceTriggered: 1
});