(function() {
  (function g(m, n) {
    if (m = n) {
      return eval("x=this")
    }
    g(m, 1)[[]]
  })()
})()
Function("\
  for (let b in [0]) {\
    for (var k = 0; k < 6; ++k) {\
      if (k == 1) {\
        print(x)\
      }\
    }\
  }\
")()

/* Don't crash/assert. */

