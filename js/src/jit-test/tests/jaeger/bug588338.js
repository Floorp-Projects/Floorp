// |jit-test| error: is not a function
function f() {
    (e)
}
(x = new Proxy(Function, (function(x) {
  return {
    get: function(r, b) {
      return x[b]
    }
  }
})(/x/)))
for (e in [0]) {
  gczeal(2)
} ( [1,2,3])("")

