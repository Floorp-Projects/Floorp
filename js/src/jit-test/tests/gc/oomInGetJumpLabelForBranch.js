// |jit-test| --no-threads
load(libdir + 'oomTest.js');
oomTest(() => getBacktrace({thisprops: gc() && delete addDebuggee.enabled}));
