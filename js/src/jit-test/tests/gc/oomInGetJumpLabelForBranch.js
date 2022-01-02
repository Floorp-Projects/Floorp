// |jit-test| skip-if: !('oomTest' in this)

oomTest(() => getBacktrace({thisprops: gc() && delete addDebuggee.enabled}));
