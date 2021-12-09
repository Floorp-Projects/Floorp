// |jit-test| skip-if: helperThreadCount() === 0
verifyprebarriers();
offThreadCompileToStencil('');
var dbg = new Debugger();
var objects = dbg.findObjects();
