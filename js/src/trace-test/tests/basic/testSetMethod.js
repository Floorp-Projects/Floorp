function C() {
    this.a = function() {};
    this.b = function() {};
}
for (var i = 0; i < 9; i++)
    new C;

checkStats({
    recorderStarted: 1,
    recorderAborted: 0,
    traceCompleted: 1,
    sideExitIntoInterpreter: 1
});
