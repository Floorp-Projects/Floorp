// Eval-in-frame with different type on baseline frame with let-scoping

load(libdir + "jitopts.js");

if (!jitTogglesMatch(Opts_BaselineEager))
  quit(0);

withJitOptions(Opts_BaselineEager, function () {
  var g = newGlobal();
  var dbg = new Debugger;

  g.h = function h(d) {
    if (d) {
      dbg.addDebuggee(g);
      var f = dbg.getNewestFrame().older;
      assertEq(f.implementation, "baseline");
      assertEq(f.environment.getVariable("foo"), 42);
      f.eval("foo = 'string of 42'");
    }
  }

  g.eval("" + function f(d) {
    if (d) {
      let foo = 42;
      g(d);
      return foo;
    }
  });

  g.eval("" + function g(d) { h(d); });

  g.eval("(" + function () { assertEq(f(true), "string of 42"); } + ")();");
});
