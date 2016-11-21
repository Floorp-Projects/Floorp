// |jit-test| error: RangeError; need-for-each
(function() {
  for each(let a in [function() {}, Infinity]) {
    new Array(a)
  }
})()

/* Don't assert/crash. */

