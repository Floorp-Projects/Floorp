// These predicates are for tests that require a particular set of JIT options.

// Check if toggles match. Useful for tests that shouldn't be run if a
// different set of JIT toggles are set, since TBPL runs each jit-test
// multiple times with a variety of flags.
function jitTogglesMatch(opts) {
  var currentOpts = getJitCompilerOptions();
  for (var k in opts) {
    if (k.indexOf(".enable") > 0 && opts[k] != currentOpts[k])
      return false;
  }
  return true;
}

// Run fn under a particular set of JIT options.
function withJitOptions(opts, fn) {
  var oldOpts = getJitCompilerOptions();
  for (var k in opts)
    setJitCompilerOption(k, opts[k]);
  try {
    fn();
  } finally {
    for (var k in oldOpts)
      setJitCompilerOption(k, oldOpts[k]);
  }
}

// N.B. Ion opts *must come before* baseline opts because there's some kind of
// "undo eager compilation" logic. If we don't set the baseline warmup-counter
// *after* the Ion warmup-counter we end up setting the baseline warmup-counter
// to be the default if we hit the "undo eager compilation" logic.
var Opts_BaselineEager =
    {
      'ion.enable': 1,
      'baseline.enable': 1,
      'baseline.warmup.trigger': 0,
      'offthread-compilation.enable': 1
    };

// Checking for offthread compilation being off is often helpful if the test
// requires a function be Ion compiled. Each individual test will usually
// finish before the Ion compilation thread has a chance to attach the
// compiled code.
var Opts_IonEagerNoOffthreadCompilation =
    {
      'ion.enable': 1,
      'ion.warmup.trigger': 0,
      'baseline.enable': 1,
      'baseline.warmup.trigger': 0,
      'offthread-compilation.enable': 0,
    };

var Opts_Ion2NoOffthreadCompilation =
    {
      'ion.enable': 1,
      'ion.warmup.trigger': 2,
      'baseline.enable': 1,
      'baseline.warmup.trigger': 1,
      'offthread-compilation.enable': 0
    };

var Opts_NoJits =
    {
      'ion.enable': 0,
      'ion.warmup.trigger': 0,
      'baseline.warmup.trigger': 0,
      'baseline.enable': 0,
      'offthread-compilation.enable': 0
    };
