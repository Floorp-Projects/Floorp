options('strict_mode');
var g1 = newGlobal();
var g2 = newGlobal();
var dbg = new Debugger();
dbg.addDebuggee(g1);
g1.eval('function f() {}');
gczeal(9, 1);
dbg.findScripts({});
