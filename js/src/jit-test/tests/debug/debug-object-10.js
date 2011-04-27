// |jit-test| debug
// If a hook is deleted after setHooks or overwritten with a primitive, it
// simply isn't called.

var g = newGlobal('new-compartment');
var hit = false;
var dbg = new Debug(g);
dbg.hooks = {debuggerHandler: function (stack) { hit = 'fail';}};

delete dbg.hooks.debuggerHandler;
g.eval("debugger;");
assertEq(hit, false);

dbg.hooks.debuggerHandler = null;
g.eval("debugger;");
assertEq(hit, false);

// Reinstating the hook should work.
dbg.hooks.debuggerHandler = function (stack) { hit = true; };
g.eval("debugger;");
assertEq(hit, true);
