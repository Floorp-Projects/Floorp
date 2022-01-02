// |jit-test| --no-blinterp; skip-if: !('oomTest' in this)

// Disable the JITs to make oomTest more reliable

oomTest(() => Object.bind())
