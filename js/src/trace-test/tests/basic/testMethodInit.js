function testMethodInit() {  // bug 503198
    function o() { return 'o'; }
    function k() { return 'k'; }

    var x;
    for (var i = 0; i < 10; i++)
        x = {o: o, k: k};
    return x.o() + x.k();
}
assertEq(testMethodInit(), "ok");
checkStats({
  recorderStarted: 1,
  traceCompleted: 1,
  sideExitIntoInterpreter: 1
});
