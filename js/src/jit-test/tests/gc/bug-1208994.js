if (!('oomTest' in this))
    quit();

oomTest(() => getBacktrace({args: oomTest[load+1], locals: true, thisprops: true}));
