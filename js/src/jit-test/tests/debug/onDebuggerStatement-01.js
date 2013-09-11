var g = newGlobal();
g.log = '';

var dbg = Debugger(g);
dbg.onDebuggerStatement = function (stack) { g.log += '!'; };
assertEq(g.eval("log += '1'; debugger; log += '2'; 3;"), 3);
assertEq(g.log, '1!2');
