var log;
function b(x) { log += 'b'; return 'B'; }
function g() {
    log = '';
    var a = {toString: function () { log += 'a'; return 'A'; }};
    assertEq("[" + a + b() + "]", "[AB]");
    assertEq(log, "ab");
}

for (var i = 0; i < 1000; ++i)
    g();

checkStats({recorderStarted:1, recorderAborted:0, traceCompleted:1});
