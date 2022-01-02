// |jit-test| skip-if: !('oomTest' in this)
oomTest(() => dumpScopeChain(eval(`b => 1`)));
