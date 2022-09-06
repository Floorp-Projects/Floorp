// |jit-test| allow-oom; skip-if: typeof oomAfterAllocations !== 'function'
lfcode = new Array();
oomAfterAllocations(100);
loadFile(file);
lfGlobal = newGlobal()
for (lfLocal in this)
  if (!(lfLocal in lfGlobal))
    lfGlobal[lfLocal] = this[lfLocal]
offThreadCompileToStencil(lfVarx);
var stencil = lfGlobal.finishOffThreadStencil();
lfGlobal.evalStencil(stencil);
