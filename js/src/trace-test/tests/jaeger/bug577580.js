// |trace-test| error: ReferenceError
(function() {
  for (; i;) {
    eval(/@/, "")
  }
})()

