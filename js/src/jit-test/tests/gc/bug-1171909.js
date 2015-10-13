if (!('oomTest' in this))
    quit();

oomTest((function(x) { assertEq(x + y + ex, 25); }));
