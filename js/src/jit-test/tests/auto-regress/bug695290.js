// |jit-test| error:Error

// Binary: cache/js-dbg-64-cf5da681d577-linux
// Flags: -m -n -a
//

options("typeinfer");
gczeal(2);
var g = newGlobal('new-compartment');
gcparam("maxBytes", 22000);
let array = g.eval("new Array(1,2,3)");
