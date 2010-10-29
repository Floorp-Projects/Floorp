outer:
for (var i = 10; i < 19; i++)
    for (var j = 0; j < 9; j++)
        if (j < i)
            continue outer;

checkStats({recorderStarted: 3, recorderAborted: 1, traceCompleted: 2});