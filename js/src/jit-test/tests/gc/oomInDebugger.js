if (!('oomTest' in this))
    quit();

var g = newGlobal();
oomTest(() => Debugger(g));
