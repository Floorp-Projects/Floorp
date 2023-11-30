// |jit-test| skip-if: helperThreadCount() === 0

// On x86, MaxCodePages is 2240. Because we sometimes leave a one-page
// gap, this will guarantee there are no free two-page chunks.
for (var i = 0; i < 2200; i++) {
  evalcx("function s(){}", evalcx('lazy'));
}

// Allocating trampolines for the JitRuntime requires two pages.
evalInWorker("");
