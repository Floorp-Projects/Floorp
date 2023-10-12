// |jit-test| skip-if: getBuildConfiguration('pbl')
// Debugger.Frames of all implementations.

load(libdir + "jitopts.js");

function testFrameImpl(jitopts, assertFrameImpl) {
  if (!jitTogglesMatch(jitopts))
    return;

  withJitOptions(jitopts, function () {
    var g = newGlobal({newCompartment: true});
    var dbg = new Debugger;

    g.toggle = function toggle(d) {
      if (d) {
        dbg.addDebuggee(g);
        var frame = dbg.getNewestFrame();
        // We only care about the f and g frames.
        for (var i = 0; i < 2; i++) {
          assertFrameImpl(frame);
          frame = frame.older;
        }
      }
    };

    g.eval("" + function f(d) { g(d); });
    g.eval("" + function g(d) { toggle(d); });

    g.eval("(" + function test() {
      for (var i = 0; i < 5; i++)
        f(false);
      f(true);
    } + ")();");
  });
}

[[Opts_BaselineEager,
  function (f) { assertEq(f.implementation, "baseline"); }],
 // Note that the Ion case *depends* on CCW scripted functions being opaque to
 // Ion optimization and not deoptimizing the frames below the call to toggle.
 [Opts_Ion2NoOffthreadCompilation,
  function (f) { assertEq(f.implementation, "ion"); }],
 [Opts_NoJits,
  function (f) { assertEq(f.implementation, "interpreter"); }]].forEach(function ([opts, fn]) {
    testFrameImpl(opts, fn);
  });
