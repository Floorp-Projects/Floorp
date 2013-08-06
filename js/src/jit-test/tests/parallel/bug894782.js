// Don't crash

if (getBuildConfiguration().parallelJS) {
  print(ParallelArray())
  String(Object.create(ParallelArray(8077, function() {})))
}
