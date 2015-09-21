// |jit-test| --no-threads

load(libdir + 'oomTest.js');
var g = newGlobal();
oomTest(() => Debugger(g));
