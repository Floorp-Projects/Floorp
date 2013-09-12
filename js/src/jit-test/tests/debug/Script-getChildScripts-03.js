// getChildScripts on a direct eval script returns the right scripts.
// (A bug had it also returning the script for the calling function.)

var g = newGlobal();
var dbg = Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    var arr = frame.script.getChildScripts();
    assertEq(arr.length, 1);
    assertEq(arr[0], frame.eval("h").return.script);
    hits++;
};

g.eval("function f(s) { eval(s); }");
g.f("debugger; function h(a) { return a + 1; }");
assertEq(hits, 1);
