// |jit-test| --no-ggc; allow-unhandlable-oom; --no-threads

load(libdir + 'oomTest.js');
oomTest(() => getBacktrace({args: true, locals: true, thisprops: true}));
