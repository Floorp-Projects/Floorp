// |jit-test| skip-if: !('oomTest' in this)

// Don't run a full oomTest because it takes ages - a few iterations are
// sufficient to trigger the bug.
let i = 0;

oomTest(Function(`
    if (i < 10) {
        i++;
        gczeal(15,1);
        foo;
    }
`));
