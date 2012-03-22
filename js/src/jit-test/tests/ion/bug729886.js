for (var i = 0; i < 100; ++i)
    dumpStack();

function deep1(x) {
    dumpStack();
}

for (var j = 0; j < 100; ++j)
    deep1(j);
