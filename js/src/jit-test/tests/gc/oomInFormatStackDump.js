// |jit-test| --no-threads

load(libdir + 'oomTest.js');
oomTest(() => getBacktrace({args: true, locals: true, thisprops: true}));
