function bug783924() {
  // Shouldn't throw.
  Function("ParallelArray([])")();
}

bug783924();
