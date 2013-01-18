// |jit-test| error:TypeError

// Binary: cache/js-dbg-32-2c8e43e57337-linux
// Flags: -j
//
function b() { this.m(); }
var g = {m: function(){}};
var a = [g, g, {}];
for (var i = 0; i < a.length; ++i)
  b.call(a[i]);
