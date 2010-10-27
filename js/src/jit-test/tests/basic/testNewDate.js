function testNewDate()
{
    // Accessing global.Date for the first time will change the global shape,
    // so do it before the loop starts; otherwise we have to loop an extra time
    // to pick things up.
    var start = new Date();
    var time = new Date();
    for (var j = 0; j < RUNLOOP; ++j)
        time = new Date();
    return time > 0 && time >= start;
}
assertEq(testNewDate(), true);
checkStats({
    recorderStarted: 1,
    recorderAborted: 0,
    traceTriggered: 1
});
