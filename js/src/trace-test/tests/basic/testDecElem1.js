var obj = {p: 100};
var name = "p";
var a = [];
for (var i = 0; i < 10; i++)
    a[i] = --obj[name];
assertEq(a.join(','), '99,98,97,96,95,94,93,92,91,90');
assertEq(obj.p, 90);

checkStats({recorderStarted: 1, recorderAborted: 0, traceCompleted: 1, traceTriggered: 1});

