assertEq(
    (function () {
	eval();
        var arr = [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 ];

        for (var i = 0; i < 4; ++i) {
            var src = i * 8;
            var dst = i * 8 + 7;
            for (var j = 0; j < 4; ++j) {
                [arr[dst--], arr[src++]] = [arr[src], arr[dst]];
            }
        }
        return arr;
    })().toSource(),
    "[7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8, 23, 22, 21, 20, 19, 18, 17, 16, 31, 30, 29, 28, 27, 26, 25, 24, 32]");

checkStats({
    recorderStarted: 2,
    traceCompleted: 2,
    sideExitIntoInterpreter: 3,
    traceTriggered: 3
});
