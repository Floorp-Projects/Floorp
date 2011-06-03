var g = newGlobal('new-compartment');
g.log = '';

var dbg = Debug(g);
var hooks = {debuggerHandler: function (stack) { g.log += '!'; }};
dbg.hooks = hooks;
assertEq(dbg.hooks, hooks);
assertEq(g.eval("log += '1'; debugger; log += '2'; 3;"), 3);
assertEq(g.log, '1!2');
