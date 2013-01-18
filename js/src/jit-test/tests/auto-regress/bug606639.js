// |jit-test| error:Error

// Binary: cache/js-dbg-64-32b049250e03-linux
// Flags: -m
//
function v(s) { eval(s); }
v("eval(function(){})()");
var x = Int32Array(0);
v("x.set()");
