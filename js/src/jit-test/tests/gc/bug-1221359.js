if (!('oomTest' in this))
    quit();

oomTest(() => getBacktrace({
    locals: true,
    thisprops: true
}));
