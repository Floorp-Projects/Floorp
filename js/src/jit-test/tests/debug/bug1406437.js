var g = newGlobal({newCompartment: true});
g.f = function() {};
g.eval('f = clone(f);');
var dbg = new Debugger;
var dg = dbg.addDebuggee(g);
dg.getOwnPropertyDescriptor('f').value.script.source;
