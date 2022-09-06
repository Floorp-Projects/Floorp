// |jit-test| allow-oom; allow-unhandlable-oom
// 1236476

if (typeof oomTest !== 'function' ||
    typeof offThreadCompileToStencil !== 'function' ||
    typeof finishOffThreadStencil !== 'function' ||
    typeof evalStencil !== 'function')
    quit();

oomTest(() => {
    offThreadCompileToStencil(`
      "use asm";
      return assertEq;
    `);
    var stencil = finishOffThreadStencil();
    evalStencil();
});

