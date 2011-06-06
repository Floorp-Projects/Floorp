// |jit-test| debug
// After hooks.throw throws, if uncaughtExceptionHook returns undefined,
// the original exception continues to propagate.

var g = newGlobal('new-compartment');
var dbg = new Debug(g);
var log = '';
dbg.hooks = {throw: function () { log += "1"; throw new Error("oops"); }};
dbg.uncaughtExceptionHook = function () { log += "2"; };

g.eval("var x = new Error('oops');");
g.eval("try { throw x; } catch (exc) { assertEq(exc, x); }");
assertEq(log, "12");
