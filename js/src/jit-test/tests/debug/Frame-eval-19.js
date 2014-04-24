// Eval-in-frame of optimized frames to break out of an infinite loop.

load(libdir + "jitopts.js");

if (!jitTogglesMatch(Opts_IonEagerNoParallelCompilation))
  quit(0);

withJitOptions(Opts_IonEagerNoParallelCompilation, function () {
  var g = newGlobal();
  var dbg = new Debugger;

  g.eval("" + function f(d) { g(d); });
  g.eval("" + function g(d) { h(d); });
  g.eval("" + function h(d) {
    var i = 0;
    while (d)
      interruptIf(d && i++ == 4000);
  });

  setInterruptCallback(function () {
    dbg.addDebuggee(g);
    var frame = dbg.getNewestFrame();
    if (frame.callee.name != "h" || frame.implementation != "ion")
      return true;
    frame.eval("d = false;");
    return true;
  });

  g.eval("(" + function () {
    for (i = 0; i < 5; i++)
      f(false);
    f(true);
  } + ")();");
});
