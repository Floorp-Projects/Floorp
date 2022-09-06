// |jit-test| skip-if: helperThreadCount() === 0

var g = newGlobal({ newCompartment: true });
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
lfOffThreadGlobal = g;
lfOffThreadGlobal.offThreadCompileToStencil(`
    grayRoot()[0] = "foo";
  `);
var stencil = lfOffThreadGlobal.finishOffThreadStencil();
lfOffThreadGlobal.evalStencil(stencil);
var g = newGlobal({newCompartment: true});
var gw = dbg.addDebuggee(g);
lfOffThreadGlobal = null;
gc();
schedulezone(this);
schedulezone('atoms');
gc('zone');

