// Test that new.target is acceptably usable in RematerializedFrames.

gczeal(0);

load(libdir + "jitopts.js");

if (!jitTogglesMatch(Opts_Ion2NoOffthreadCompilation))
  quit();

withJitOptions(Opts_Ion2NoOffthreadCompilation, function () {
  var g = newGlobal();
  var dbg = new Debugger;

  g.toggle = function toggle(d, expected) {
    if (d) {
      dbg.addDebuggee(g);

      var frame = dbg.getNewestFrame();
      assertEq(frame.implementation, "ion");
      assertEq(frame.constructing, true);

      // CONGRATS IF THIS FAILS! You, proud saviour, have made new.target parse
      // in debug frame evals (presumably by hooking up static scope walks).
      // Uncomment the assert below for efaust's undying gratitude.
      // Note that we use .name here because of CCW nonsense.
      assertEq(frame.eval('new.target').throw.unsafeDereference().name, "SyntaxError");
      // assertEq(frame.eval('new.target').value.unsafeDereference(), expected);
    }
  };

  g.eval("" + function f(d) { new g(d, g, 15); });

  g.eval("" + function g(d, expected) { toggle(d, expected); });

  g.eval("(" + function test() {
    for (var i = 0; i < 5; i++)
      f(false);
    f(true);
  } + ")();");
});
