// Bug 1026477: Defining functions with D.F.p.eval works, even if there's
// already a non-aliased var binding for the identifier.

var g = newGlobal();
var dbg = new Debugger(g);
dbg.onDebuggerStatement = function (frame) {
  frame.older.eval('function f() { }');
};

// When the compiler sees the 'debugger' statement, it marks all variables as
// aliased, but we want to test the case where f is in a stack frame slot, so we
// put the 'debugger' statement in a separate function, and use frame.older to
// get back to the anonymous function's frame.
g.eval('function q() { debugger; }');
assertEq(typeof g.eval('(function () { var f = 42; q(); return f; })();'),
         "function");
