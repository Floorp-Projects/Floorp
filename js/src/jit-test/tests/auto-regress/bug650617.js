// |jit-test| error:ReferenceError

// Binary: cache/js-dbg-64-242947d76f73-linux
// Flags:
//
for (b in [evalcx("let(e)eval()", evalcx('split'))]) {}
