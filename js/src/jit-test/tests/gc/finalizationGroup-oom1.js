// |jit-test| --enable-weak-refs; skip-if: !('oomTest' in this)

// Don't test prototype initialization etc.
new FinalizationGroup(x => 0);

oomTest(() => {
    new FinalizationGroup(x => 0);
});
