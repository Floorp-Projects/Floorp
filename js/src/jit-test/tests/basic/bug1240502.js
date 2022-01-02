// |jit-test| skip-if: !('oomTest' in this)
oomTest(() => eval(`Array(..."ABC")`));
