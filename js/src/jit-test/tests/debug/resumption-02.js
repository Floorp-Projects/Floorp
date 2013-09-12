// Simple {return:} resumption.

var g = newGlobal();
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (stack) { return {return: 1234}; };

assertEq(g.eval("debugger; false;"), 1234);
g.eval("function f() { debugger; return 'bad'; }");
assertEq(g.f(), 1234);
