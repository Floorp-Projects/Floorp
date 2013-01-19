// |jit-test| error:TypeError

// Binary: cache/js-dbg-64-66c8ad02543b-linux
// Flags:
//
var handler = { fix: function() { return []; } };
var p = Proxy.createFunction(handler, function(){}, function(){});
Proxy.fix(p);
new p();
