(function() {
  ((function f(a) {
    if (a > 0) {
      f(a - 1)
    }
  })(6))
})()

