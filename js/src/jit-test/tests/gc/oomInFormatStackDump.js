if (!('oomTest' in this))
    quit();

oomTest(() => getBacktrace({args: true, locals: true, thisprops: true}));
