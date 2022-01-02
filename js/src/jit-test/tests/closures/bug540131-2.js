// |jit-test| error: TypeError
(function() {
  for (let z in [true]) {
    (new(eval("for(l in[0,0,0,0]){}"))
     (((function f(a, b) {
      if (a.length == b) {
        return (z)
      }
      f(a, b + 1)
    })([,,], 0)), []))
  }
})()

