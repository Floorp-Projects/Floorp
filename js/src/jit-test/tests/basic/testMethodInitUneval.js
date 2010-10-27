for (var j = 0; j < 7; ++j)
    uneval({x: function () {}});

checkStats({
    recorderStarted: 1,
    recorderAborted: 0,
    traceCompleted: 1,
    sideExitIntoInterpreter: 4
});
