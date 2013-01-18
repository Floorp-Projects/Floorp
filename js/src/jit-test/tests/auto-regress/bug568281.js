// |jit-test| error:TypeError

// Binary: cache/js-dbg-64-9ca0a738a8ad-linux
// Flags:
//
__defineSetter__("x", Array.reduce)
x = Proxy.create(function() {},
this.watch("x",
function() {
  yield
}))
