gczeal(9, 2)
var g1 = newGlobal({newCompartment: true});
var g2 = newGlobal({newCompartment: true});
var dbg = new Debugger();
var g1w = dbg.addDebuggee(g1);
g1.eval('function f() {}');
scripts = dbg.findScripts({});
