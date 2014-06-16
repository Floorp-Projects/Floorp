// Ion can bail in-place when throwing exceptions with debug mode toggled on.

load(libdir + "jitopts.js");

if (!jitTogglesMatch(Opts_Ion2NoOffthreadCompilation))
  quit();

withJitOptions(Opts_Ion2NoOffthreadCompilation, function () {
  var g = newGlobal();
  var dbg = new Debugger;

  g.toggle = function toggle(x, d) {
    if (d) {
      dbg.addDebuggee(g);
      var frame = dbg.getNewestFrame().older;
      assertEq(frame.callee.name, "f");
      assertEq(frame.implementation, "ion");
      throw 42;
    }
  };

  g.eval("" + function f(x, d) { g(x, d); });
  g.eval("" + function g(x, d) { toggle(x, d); });

  try {
    g.eval("(" + function test() {
      for (var i = 0; i < 5; i++)
        f(42, false);
      f(42, true);
    } + ")();");
  } catch (exc) {
    assertEq(exc, 42);
  }
});
