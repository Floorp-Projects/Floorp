for (var i = 0; i < 9; i++) {
    var x = {f: function() {}};
    x.f++;
    assertEq(""+x.f, "NaN");
}

checkStats({
    recorderStarted: 1,
    recorderAborted: 1,
    traceCompleted: 0,
    sideExitIntoInterpreter: 0
});
