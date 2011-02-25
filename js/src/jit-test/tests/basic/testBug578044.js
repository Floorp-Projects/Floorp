this.watch("x", Object.create)
try {
  (function() {
    this.__defineGetter__("x",
    function() {
      return this
    })
  })()
} catch(e) {}
Object.defineProperty(x, "x", ({
  set: Uint16Array
}))

