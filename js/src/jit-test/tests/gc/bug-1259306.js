// |jit-test| skip-if: !('oomTest' in this)

let runCount = 0;
oomTest(() => {
    if (runCount < 5) {
        let lfGlobal = newGlobal();
        var lfVarx = `
            gczeal(8, 1);
            try {
                (5).replace(r, () => {});
            } catch (x) {}
            gczeal(0);
        `;
        lfGlobal.offThreadCompileToStencil(lfVarx);
        var stencil = lfGlobal.finishOffThreadStencil();
        lfGlobal.evalStencil(stencil);
        runCount++;
    }
});
