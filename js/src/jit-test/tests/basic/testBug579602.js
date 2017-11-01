// don't panic

f = function*() {
  x = yield
}
rv = f()
for (a of rv) (function() {})
x = new Proxy({}, (function() {
  return {
    defineProperty: gc
  }
})());
with({
  d: (({
    x: Object.defineProperty(x, "", ({
      set: Array.e
    }))
  }))
}) {}

// don't crash
