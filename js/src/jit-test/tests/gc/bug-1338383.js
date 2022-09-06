// |jit-test| error: InternalError

if (helperThreadCount() === 0)
    throw InternalError();

var lfOffThreadGlobal = newGlobal();
enableShellAllocationMetadataBuilder()
lfOffThreadGlobal.offThreadCompileToStencil(`
  if ("gczeal" in this)
    gczeal(8, 1)
  function recurse(x) {
    recurse(x + 1);
  };
  recurse(0);
`);
var stencil = lfOffThreadGlobal.finishOffThreadStencil();
lfOffThreadGlobal.evalStencil(stencil);
