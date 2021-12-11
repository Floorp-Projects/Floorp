// |jit-test| skip-if: !('oomTest' in this)

oomTest(() => getBacktrace({args: oomTest[load+1], locals: true, thisprops: true}));
