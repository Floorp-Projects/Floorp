// The arguments can escape from a function via a debugging hook.

var g = newGlobal('new-compartment');
var dbg = new Debugger(g);

// capture arguments object and test function
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    try {
        frame.older.environment.parent.getVariable('arguments')
    } catch (e) {
        assertEq(''+e, "Error: Debugger scope is not live");
        hits++;
    }
};
g.eval("function h() { debugger; }");
g.eval("function f() { var x = 0; return function() { x++; h() } }");
g.eval("f('ponies')()");
assertEq(hits, 1);
