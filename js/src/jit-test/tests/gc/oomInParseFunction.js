// |jit-test| --no-threads

load(libdir + 'oomTest.js');
oomTest(() => eval("function f() {}"));
