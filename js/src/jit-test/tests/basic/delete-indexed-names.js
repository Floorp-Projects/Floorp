var a = ['p', 'q', 'r', 's', 't'];
var o = {p:1, q:2, r:3, s:4, t:5};
for (var i in o)
    delete o[i];
for each (var i in a)
    assertEq(o.hasOwnProperty(i), false);

checkStats({
    recorderAborted:0,
    traceCompleted:2,
    sideExitIntoInterpreter:2
});
