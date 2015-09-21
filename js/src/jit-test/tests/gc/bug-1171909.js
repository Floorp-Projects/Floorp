// |jit-test| --no-threads; allow-unhandlable-oom
load(libdir + 'oomTest.js');
oomTest((function(x) { assertEq(x + y + ex, 25); }));
