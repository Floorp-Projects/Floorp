
setJitCompilerOption('ion.warmup.trigger', 2);
setJitCompilerOption('offthread-compilation.enable', 0);
var g = newGlobal();
var dbg2 = new Debugger;
g.toggle = function toggle(x, d) {
  if (d) {
    dbg2.addDebuggee(g);
    dbg2.getNewestFrame().environment.getVariable("x");
  }
};
g.eval("" + function f(x, d) { toggle(++arguments, d); });
g.eval("(" + function test() {
  for (var i = 0; i < 30; i++)
    f(42, false);
  f(42, true);
} + ")();");
