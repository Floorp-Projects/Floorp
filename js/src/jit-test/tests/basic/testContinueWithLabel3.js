// bug 511575 comment 3
outer:
for (var j = 0; j < 10; j++)
    for (var p in {x:1})
        if (p > "q")
            continue outer;
