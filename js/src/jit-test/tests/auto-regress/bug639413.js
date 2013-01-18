// |jit-test| error:TypeError

// Binary: cache/js-dbg-32-1c8e91b2e3a4-linux
// Flags:
//
a = evalcx("lazy");
a.watch("x", function() {});
({}).watch("x", function() {});
a.__defineGetter__("y", {});
