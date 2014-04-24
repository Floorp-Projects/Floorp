// Debugger.Frame preserves Ion frame identity

load(libdir + "jitopts.js");

if (!jitTogglesMatch(Opts_Ion2NoParallelCompilation))
  quit();

withJitOptions(Opts_Ion2NoParallelCompilation, function () {
  var g = newGlobal();
  var dbg1 = new Debugger;
  var dbg2 = new Debugger;

  g.toggle = function toggle(x, d) {
    if (d) {
      dbg1.addDebuggee(g);
      dbg2.addDebuggee(g);
      var frame1 = dbg1.getNewestFrame();
      assertEq(frame1.environment.getVariable("x"), x);
      assertEq(frame1.implementation, "ion");
      frame1.environment.setVariable("x", "not 42");
      assertEq(dbg2.getNewestFrame().environment.getVariable("x"), "not 42");
    }
  };

  g.eval("" + function f(x, d) { toggle(x, d); });

  g.eval("(" + function test() {
    for (var i = 0; i < 5; i++)
      f(42, false);
    f(42, true);
  } + ")();");
});
