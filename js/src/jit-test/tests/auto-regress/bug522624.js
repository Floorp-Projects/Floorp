// Binary: cache/js-dbg-32-a847cf5b4669-linux
// Flags: -j
//
function r([]) { r(); }
var a = {};
a.__defineGetter__("t", r);
try { a.t; } catch(e) { }
print(uneval(a));
