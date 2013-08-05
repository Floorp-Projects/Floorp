// Don't crash

if (getBuildConfiguration().parallelJS) {
  Object.defineProperty(this, "y", {
    get: function() {
      return Object.create(x)
    }
  })
  x = ParallelArray([1142], function() {})
  x = x.partition(2)
  y + y
}
