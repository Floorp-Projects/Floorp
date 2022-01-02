// |jit-test| error:Error

var g = newGlobal();
var dbg = new Debugger(g);
dbg.onNewScript = function(script) {
  fscript = script.getChildScripts()[0];
}
g.eval("function f(x) { arguments[0] = 3; return x }");
fscript.setBreakpoint(0, {hit:function(frame) {
  assertEq(frame.eval("assertEq(arguments, undefined)").return, 1);
}});
assertEq(g.f(1), 42);
