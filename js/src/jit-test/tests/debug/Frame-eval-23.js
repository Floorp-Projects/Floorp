// Debugger.Frame preserves Ion frame mutations after removing debuggee.

load(libdir + "jitopts.js");

if (!jitTogglesMatch(Opts_Ion2NoParallelCompilation))
  quit();

withJitOptions(Opts_Ion2NoParallelCompilation, function () {
  var g = newGlobal();
  var dbg = new Debugger;

  g.toggle = function toggle(x, d) {
    if (d) {
      dbg.addDebuggee(g);
      var frame = dbg.getNewestFrame().older;
      assertEq(frame.callee.name, "f");
      assertEq(frame.environment.getVariable("x"), x);
      assertEq(frame.implementation, "ion");
      frame.environment.setVariable("x", "not 42");
      dbg.removeDebuggee(g);
    }
  };

  g.eval("" + function f(x, d) {
    g(x, d);
    if (d)
      assertEq(x, "not 42");
  });

  g.eval("" + function g(x, d) { toggle(x, d); });

  g.eval("(" + function test() {
    for (var i = 0; i < 5; i++)
      f(42, false);
    f(42, true);
  } + ")();");
});
