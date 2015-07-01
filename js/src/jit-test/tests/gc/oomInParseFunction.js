// |jit-test| --no-ggc; allow-unhandlable-oom

load(libdir + 'oomTest.js');
oomTest(() => eval("function f() {}"));
