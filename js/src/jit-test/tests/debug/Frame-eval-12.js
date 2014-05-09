// The arguments can escape from a function via a debugging hook.

var g = newGlobal();
var dbg = new Debugger(g);

// capture arguments object and test function
dbg.onDebuggerStatement = function (frame) {
  var args = frame.older.environment.parent.getVariable('arguments');
  assertEq(args.missingArguments, true);
};
g.eval("function h() { debugger; }");
g.eval("function f() { var x = 0; return function() { x++; h() } }");
g.eval("f('ponies')()");
