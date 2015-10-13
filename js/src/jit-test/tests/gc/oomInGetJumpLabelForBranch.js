// |jit-test| allow-oom; allow-unhandlable-oom; --no-threads
if (!('oomTest' in this))
    quit();

oomTest(() => getBacktrace({thisprops: gc() && delete addDebuggee.enabled}));
