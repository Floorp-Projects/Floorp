// Looking at ScriptSourceObjects in invisible-to-debugger compartments is okay.

var gi = newGlobal({ newCompartment: true, invisibleToDebugger: true });
gi.eval('function f() {}');

var gv = newGlobal({newCompartment: true});
gv.f = gi.f;
gv.eval('f = clone(f);');

var dbg = new Debugger;
var gvw = dbg.addDebuggee(gv);
gvw.getOwnPropertyDescriptor('f').value.script.source;
