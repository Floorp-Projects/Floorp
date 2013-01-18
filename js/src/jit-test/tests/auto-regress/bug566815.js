// |jit-test| error:TypeError

// Binary: cache/js-dbg-64-6fc5d661ca55-linux
// Flags:
//
x = /x/
x.__proto__ = new Namespace
x > 0
