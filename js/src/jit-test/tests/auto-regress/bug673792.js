// |jit-test| error:ReferenceError

// Binary: cache/js-dbg-32-8c7adf094b8e-linux
// Flags:
//
a = {}.__proto__
gc(evalcx('split'))
