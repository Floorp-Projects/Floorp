// |jit-test| allow-oom

if (!('oomTest' in this))
    quit();

evalcx('oomTest(function() { Array(...""); })', newGlobal());
