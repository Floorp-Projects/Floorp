for (var i = 0; i < 9; i++)
    x = {a: function() {}, b: function() {}};

checkStats({
    recorderStarted: 1,
    recorderAborted: 0,
    traceCompleted: 1,
    sideExitIntoInterpreter: 1
});
