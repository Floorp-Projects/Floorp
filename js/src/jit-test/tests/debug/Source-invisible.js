// Looking at ScriptSourceObjects in invisible-to-debugger compartments is okay.

var gi = newGlobal({ newCompartment: true, invisibleToDebugger: true });

var gv = newGlobal({newCompartment: true});
gi.cloneAndExecuteScript('function f() {}', gv);

var dbg = new Debugger;
var gvw = dbg.addDebuggee(gv);
gvw.getOwnPropertyDescriptor('f').value.script.source;
