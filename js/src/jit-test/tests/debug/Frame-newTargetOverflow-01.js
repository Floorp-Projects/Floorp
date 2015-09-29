// Test that Ion frames are invalidated by turning on Debugger. Invalidation
// is unobservable, but we know that Ion scripts cannot handle Debugger
// handlers, so we test for the handlers being called.

load(libdir + "jitopts.js");

if (!jitTogglesMatch(Opts_Ion2NoOffthreadCompilation))
  quit();

// GCs can invalidate JIT code and cause this test to fail.
if ('gczeal' in this)
    gczeal(0);

withJitOptions(Opts_Ion2NoOffthreadCompilation, function () {
  var g = newGlobal();
  var dbg = new Debugger;

  g.toggle = function toggle(d) {
    if (d) {
      dbg.addDebuggee(g);

      var frame = dbg.getNewestFrame();
      assertEq(frame.implementation, "ion");
      assertEq(frame.constructing, true);

      // overflow args are read from the parent's frame
      // ensure we have the right offset to read from those.
      assertEq(frame.arguments[1], 15);
    }
  };

  g.eval("" + function f(d) { new g(d, 15); });

  g.eval("" + function g(d) { toggle(d); });

  g.eval("(" + function test() {
    for (var i = 0; i < 5; i++)
      f(false);
    f(true);
  } + ")();");
});
