if (!('oomTest' in this))
    quit();

oomTest(() => getBacktrace({thisprops: gc() && delete addDebuggee.enabled}));
