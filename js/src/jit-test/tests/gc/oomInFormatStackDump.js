// |jit-test| skip-if: !('oomTest' in this)

oomTest(() => getBacktrace({args: true, locals: true, thisprops: true}));
