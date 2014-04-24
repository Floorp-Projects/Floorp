// Tests that we can use debug scopes with Ion frames.
//
// Unfortunately these tests are brittle. They depend on opaque JIT heuristics
// kicking in.

load(libdir + "jitopts.js");

if (!jitTogglesMatch(Opts_Ion2NoParallelCompilation))
  quit(0);

withJitOptions(Opts_Ion2NoParallelCompilation, function () {
  var g = newGlobal();
  var dbg = new Debugger;

  // Note that this *depends* on CCW scripted functions being opaque to Ion
  // optimization and not deoptimizing the frames below the call to toggle.
  g.toggle = function toggle(d) {
    if (d) {
      dbg.addDebuggee(g);
      var frame = dbg.getNewestFrame();
      assertEq(frame.implementation, "ion");
      // g is heavyweight but its call object is optimized out, because its
      // arguments and locals are unaliased.
      //
      // Calling frame.environment here should make a fake debug scope that
      // gets things directly from the frame. Calling frame.arguments doesn't
      // go through the scope object and reads directly off the frame. Assert
      // that the two are equal.
      assertEq(frame.environment.getVariable("x"), frame.arguments[1]);
    }
  };

  g.eval("" + function f(d, x) { g(d, x); });
  g.eval("" + function g(d, x) {
    for (var i = 0; i < 200; i++);
    function inner() { i = 42; };
    toggle(d);
    // Use x so it doesn't get optimized out.
    x++;
  });

  g.eval("(" + function test() {
    for (i = 0; i < 5; i++)
      f(false, 42);
    f(true, 42);
  } + ")();");
});
