var n = 0;
var a = {set p(x) { n += x; }};
for (var i = 0; i < 9; i++)
    a.p = i;
assertEq(n, 36);

checkStats({recorderStarted: 1, recorderAborted: 0, traceCompleted: 1, traceTriggered: 1});
