// |jit-test| error:Error

// Binary: cache/js-dbg-32-f3f5d8a8a473-linux
// Flags:
//
function caller(obj) {}
var pc = line2pc(caller, pc2line(caller, 0XeBebb ) + 2);
