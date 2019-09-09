// |jit-test| skip-if: helperThreadCount() === 0

var g = newGlobal({ newCompartment: true });
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
lfOffThreadGlobal = g;
lfOffThreadGlobal.offThreadCompileScript(`
    grayRoot()[0] = "foo";
  `);
lfOffThreadGlobal.runOffThreadScript();
var g = newGlobal({newCompartment: true});
var gw = dbg.addDebuggee(g);
lfOffThreadGlobal = null;
gc();
schedulezone(this);
schedulezone('atoms');
gc('zone');

