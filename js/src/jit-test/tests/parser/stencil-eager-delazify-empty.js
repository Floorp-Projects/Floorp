// |jit-test| skip-if: helperThreadCount() === 0 || isLcovEnabled()

// Extra GCs can empty the StencilCache to reclaim memory. This lines
// re-configure the gc-zeal setting to prevent this from happening in this test
// case which waits for the cache to contain some entry.
if ('gczeal' in this)
  gczeal(0);

// Generate a source code with no inner functions.
function justVariables(n) {
    let text = "";
    for (let i = 0; i < n; i++) {
        text += `let v${i} = ${i};`;
    }
    return text;
};

// Create an extra task to test the case where there is nothing to delazify.
let job = offThreadCompileToStencil(justVariables(10000), options);

const stencil = finishOffThreadStencil(job);
evalStencil(stencil, options);
