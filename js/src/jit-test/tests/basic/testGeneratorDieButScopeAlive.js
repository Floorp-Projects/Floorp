var g = newGlobal('new-compartment');
var dbg = new Debugger(g);


var hits = 0;
dbg.onDebuggerStatement = function(frame) {
    ++hits;
    frame.older.eval("escaped = function() { return y }");
}

g.escaped = undefined;
g.eval("function h() { debugger }");
g.eval("(function () { var y = 42; h(); yield })().next();");
assertEq(g.eval("escaped()"), 42);
gc();
assertEq(g.eval("escaped()"), 42);
