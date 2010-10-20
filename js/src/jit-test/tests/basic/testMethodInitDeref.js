for (var j = 0; j < 7; ++j)
    ({x: function () {}}).x;

checkStats({
    recorderStarted: 1,
    recorderAborted: 0,
    traceCompleted: 1,
    sideExitIntoInterpreter: 1
});
