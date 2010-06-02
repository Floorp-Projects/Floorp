var n = 0;
Object.defineProperty(this, "p", {get: function () { return n; },
                                  set: function (x) { n += x; }});
for (var i = 0; i < 9; i++)
    p = i;
assertEq(n, 36);

checkStats({recorderStarted: 1, recorderAborted: 0, traceCompleted: 1, traceTriggered: 1});
