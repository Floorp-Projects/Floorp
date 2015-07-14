// |jit-test| --no-ggc; allow-unhandlable-oom
load(libdir + 'oomTest.js');
oomTest((function(x) { assertEq(x + y + ex, 25); }));
