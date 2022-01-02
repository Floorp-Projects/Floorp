var g = newGlobal({newCompartment: true});
evaluate('function f() {}', {global: g});
var dbg = new Debugger;
var dg = dbg.addDebuggee(g);
dg.getOwnPropertyDescriptor('f').value.script.source;
