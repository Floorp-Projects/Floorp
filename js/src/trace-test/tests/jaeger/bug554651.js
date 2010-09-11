// |trace-test| error: InternalError
(function() {
  try {
    (Function("__defineGetter__(\"x\",(Function(\"for(z=0;z<6;z++)(x)\")))"))()
  } catch(e) {}
})()
((function f(d, aaaaaa) {
  if (bbbbbb = aaaaaa) {
    x
  }
  f(bbbbbb, aaaaaa + 1)
})([], 0))

/* Don't assert (32-bit mac only, relies on very specific stack usage). */

