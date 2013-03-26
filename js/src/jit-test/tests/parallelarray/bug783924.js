function bug783924() {
  // Shouldn't throw.
  Function("ParallelArray([])")();
}

if (getBuildConfiguration().parallelJS)
  bug783924();
