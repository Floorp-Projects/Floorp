function testBug458838() {
    var a = 1;
    function g() {
        var b = 0
            for (var i = 0; i < 10; ++i) {
                b += a;
            }
        return b;
    }

    return g();
}
assertEq(testBug458838(), 10);
checkStats({
  recorderStarted: 1,
  recorderAborted: 0,
  traceCompleted: 1
});
