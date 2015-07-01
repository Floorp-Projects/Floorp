// Don't hand out internal function objects via Debugger.Environment.prototype.getVariable.

// When the real scope chain object holding the binding for 'f' in 'function f()
// { ... }' is optimized out because it's never used, we whip up fake scope
// chain objects for Debugger to use, if it looks. However, the value of the
// variable f will be an internal function object, not a live function object,
// since the latter was not recorded. Internal function objects should not be
// exposed via Debugger.

var g = newGlobal();
var dbg = new Debugger(g);

dbg.onDebuggerStatement = function (frame) {
  var g_call_env = frame.older.environment;     // g's locals
  var g_decl_env = g_call_env.parent;           // 'function g' binding
  var f_call_env = g_decl_env.parent;           // f's locals
  var f_decl_env = f_call_env.parent;           // 'function f' binding
  assertEq(f_decl_env.getVariable('f').optimizedOut, true);
}

g.evaluate(`

           function h() { debugger; }
           (function f() {
             return function g() {
               h();
               return 1;
             }
           })()();

           `);
