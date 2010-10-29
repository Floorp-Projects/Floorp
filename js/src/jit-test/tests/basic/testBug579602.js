// don't panic

f = function() {
  x = yield
}
rv = f()
for (a in rv) (function() {})
x = Proxy.create((function() {
  return {
    defineProperty: gc
  }
})(), x)
with({
  d: (({
    x: Object.defineProperty(x, "", ({
      set: Array.e
    }))
  }))
}) {}

// don't crash
