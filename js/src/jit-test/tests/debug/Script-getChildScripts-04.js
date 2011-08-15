// script.getChildScripts() works correctly during the newScript hook.
// (A bug had it including the script for the calling function.)

var g = newGlobal('new-compartment');
g.eval("function h(a) { eval(a); }");

var dbg = Debugger(g);
var arr, kscript;
dbg.onNewScript = function (script) { arr = script.getChildScripts(); };
dbg.onDebuggerStatement = function (frame) { kscript = frame.callee.script; };

g.h("function k(a) { debugger; return a + 1; } k(-1);");
assertEq(kscript instanceof Debugger.Script, true);
assertEq(arr.length, 1);
assertEq(arr[0], kscript);
