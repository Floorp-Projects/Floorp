// |jit-test| allow-oom; allow-unhandlable-oom; --no-threads
load(libdir + 'oomTest.js');
oomTest(() => getBacktrace({thisprops: gc() && delete addDebuggee.enabled}));
