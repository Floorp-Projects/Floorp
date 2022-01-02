// |jit-test| skip-if: !('oomTest' in this)

// Don't test prototype initialization etc.
new FinalizationRegistry(x => 0);

oomTest(() => {
    new FinalizationRegistry(x => 0);
});
