function reallyDeepNestedExit(schedule)
{
    var c = 0, j = 0;
    for (var i = 0; i < 5; i++) {
        for (j = 0; j < 4; j++) {
            c += (schedule[i*4 + j] == 1) ? 1 : 2;
        }
    }
    return c;
}
function testReallyDeepNestedExit()
{
    var c = 0;
    var schedule1 = new Array(5*4);
    var schedule2 = new Array(5*4);
    for (var i = 0; i < 5*4; i++) {
        schedule1[i] = 0;
        schedule2[i] = 0;
    }
    /**
     * First innermost compile: true branch runs through.
     * Second '': false branch compiles new loop edge.
     * First outer compile: expect true branch.
     * Second '': hit false branch.
     */
    schedule1[0*4 + 3] = 1;
    var schedules = [schedule1,
                     schedule2,
                     schedule1,
                     schedule2,
                     schedule2];

    for (var i = 0; i < 5; i++) {
        c += reallyDeepNestedExit(schedules[i]);
    }
    return c;
}
assertEq(testReallyDeepNestedExit(), 198);
