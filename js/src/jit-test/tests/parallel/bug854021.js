// Don't crash.
if (getBuildConfiguration().parallelJS)
  ParallelArray(7, function ([y]) {})
