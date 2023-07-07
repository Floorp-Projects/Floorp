"use strict";
var g1 = newGlobal({newCompartment: true});
var g2 = newGlobal({newCompartment: true});
var dbg = new Debugger();
dbg.addDebuggee(g1);
g1.eval('function f() {}');
gczeal(9, 1);
dbg.findScripts({});
