// |jit-test| error:TypeError

// Binary: cache/js-dbg-64-4e9567eeb09e-linux
// Flags:
//
evaluate({
    e: [].some(Proxy.create(function() {}), "")
})
