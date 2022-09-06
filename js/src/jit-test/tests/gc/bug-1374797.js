// |jit-test| skip-if: helperThreadCount() === 0

// Exercise triggering GC of atoms zone while off-thread parsing is happening.

gczeal(0);

// Reduce some GC parameters so that we can trigger a GC more easily.
gcparam('lowFrequencyHeapGrowth', 120);
gcparam('highFrequencyLargeHeapGrowth', 120);
gcparam('highFrequencySmallHeapGrowth', 120);
gcparam('allocationThreshold', 1);
gc();

// Start an off-thread parse.
offThreadCompileToStencil("print('Finished')");

// Allocate lots of atoms, parsing occasionally.
for (let i = 0; i < 10; i++) {
    print(i);
    for (let j = 0; j < 10000; j++)
        Symbol.for(i + 10 * j);
    eval(`${i}`);
}

// Finish the off-thread parse.
var stencil = finishOffThreadStencil();
evalStencil(stencil);
