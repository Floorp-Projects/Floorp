var g = newGlobal({newCompartment: true});
cloneAndExecuteScript('function f() {}', g);
var dbg = new Debugger;
var dg = dbg.addDebuggee(g);
dg.getOwnPropertyDescriptor('f').value.script.source;
