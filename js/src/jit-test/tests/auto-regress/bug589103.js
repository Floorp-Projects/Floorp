// |jit-test| error:InternalError

// Binary: cache/js-dbg-64-b22e82ce2364-linux
// Flags:
//
print(__proto__ = Proxy.create(this, ""))
