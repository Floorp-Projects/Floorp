// |jit-test| skip-if: !('oomTest' in this)

oomTest(() => getBacktrace({
    locals: true,
    thisprops: true
}));
