// |jit-test| allow-oom; allow-unhandlable-oom
// 1236476

if (typeof oomTest !== 'function' ||
    typeof offThreadCompileToStencil !== 'function' ||
    typeof finishOffThreadCompileToStencil !== 'function' ||
    typeof evalStencil !== 'function')
    quit();

oomTest(() => {
    offThreadCompileToStencil(`
      "use asm";
      return assertEq;
    `);
    var stencil = finishOffThreadCompileToStencil();
    evalStencil();
});

