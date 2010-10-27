function testString() {
    var q;
    for (var i = 0; i <= RUNLOOP; ++i) {
        q = [];
        q.push(String(void 0));
        q.push(String(true));
        q.push(String(5));
        q.push(String(5.5));
        q.push(String("5"));
        q.push(String([5]));
    }
    return q.join(",");
}
assertEq(testString(), "undefined,true,5,5.5,5,5");
checkStats({
    recorderStarted: 1,
    sideExitIntoInterpreter: 1
});
