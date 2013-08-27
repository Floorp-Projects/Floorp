if (getBuildConfiguration().parallelJS) {
  x = ParallelArray([9937], function() {
      return /x/
      })
  Object.defineProperty(this, "y", {
    get: function() {
      return Object.create(x)
    }
  })
  y + x
  x = x.scatter([]);
  + y
}
