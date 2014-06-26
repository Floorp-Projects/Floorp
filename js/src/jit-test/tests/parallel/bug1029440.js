if (getBuildConfiguration().parallelJS) {
  Array.buildPar(3, function() {})
  gczeal(10, 3)
  Array.buildPar(9, function() {
      Array.prototype.unshift.call([], undefined)
  })
}
