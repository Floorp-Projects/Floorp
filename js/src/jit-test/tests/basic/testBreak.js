for (var i = 10; i < 19; i++)
    for (var j = 0; j < 9; j++)
        if (j < i)
            break;

checkStats({recorderStarted: 3, recorderAborted: 1, traceCompleted: 2});
