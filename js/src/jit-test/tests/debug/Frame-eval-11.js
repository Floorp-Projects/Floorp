// The arguments can escape from a function via a debugging hook.

var g = newGlobal();
var dbg = new Debugger(g);

// capture arguments object and test function
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    assertEq(frame.older.eval('arguments[0]').return, 'ponies');
    hits++;
};
g.eval("function g() { debugger; }");
g.eval("function f() { g(); }");
g.eval("f('ponies')");
assertEq(hits, 1);
