var a = [0,1,2,3,4,5,6,7,8,9,10];
for (var i = 0; i < 10; i++)
    delete a.length;
assertEq(delete a.length, false);

checkStats({
    recorderAborted:0,
    traceCompleted:1,
    sideExitIntoInterpreter:1
});
