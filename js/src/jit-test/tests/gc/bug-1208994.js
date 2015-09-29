// |jit-test| --no-threads

load(libdir + 'oomTest.js');
oomTest(() => getBacktrace({args: oomTest[load+1], locals: true, thisprops: true}));
