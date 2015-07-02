// |jit-test| --no-ggc; allow-unhandlable-oom

load(libdir + 'oomTest.js');
var g = newGlobal();
oomTest(() => Debugger(g));
