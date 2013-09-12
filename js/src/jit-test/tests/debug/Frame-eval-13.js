// The debugger may add new bindings into existing scopes

var g = newGlobal();
var dbg = new Debugger(g);
dbg.onDebuggerStatement = function(frame) {
    assertEq(frame.eval("var x = 3; x").return, 3);
    hits++;
}
var hits = 0;
g.eval("(function() { debugger; })()");
assertEq(hits, 1);
g.eval("(function() { var x = 4; debugger; })()");
assertEq(hits, 2);
