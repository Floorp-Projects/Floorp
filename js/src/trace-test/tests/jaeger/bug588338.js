// |trace-test| error: <x/> is not a function
function f() { (e)
} (x = Proxy.createFunction((function(x) {
  return {
    get: function(r, b) {
      return x[b]
    }
  }
})(/x/), wrap))
for (z = 0; z < 100; x.unwatch(), z++)
for (e in [0]) {
  gczeal(2)
} ( <x/>)("")

