// |jit-test| error: RangeError
(function() {
  for each(let a in [function() {}, Infinity]) {
    new Array(a)
  }
})()

/* Don't assert/crash. */

