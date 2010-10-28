function Q() {
    this.m = function () {};
}
function callm(q) {
    q.m();
}
var a = [];
for (var i = 0; i < 5; i++) {
    var q = new Q;
    callm(q);
    q.m = function () { return 42; };
    a[i] = q;
}
for (var i = 0; i < 5; i++)
    callm(a[i]);
checkStats({
    recorderStarted: 2,
    traceCompleted: 3,
    sideExitIntoInterpreter: 4
});
