// |jit-test| error:TypeError: can't redefine
var g = newGlobal({newCompartment: true});
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);
nukeAllCCWs();
gw.defineProperty("undefined", {value: -1});
