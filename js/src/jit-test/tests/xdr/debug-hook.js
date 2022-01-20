var g = newGlobal({ newCompartment: true });
var dbg = new Debugger(g);
var count = 0;

dbg.onNewScript = script => {
  count++;
};

var stencil = g.compileToStencil(`"a"`);
g.evalStencil(stencil);
assertEq(count, 1);

count = 0;
dbg.onNewScript = script => {
  count++;
};

var stencilXDR = g.compileToStencilXDR(`"b"`);
g.evalStencilXDR(stencilXDR);
assertEq(count, 1);
