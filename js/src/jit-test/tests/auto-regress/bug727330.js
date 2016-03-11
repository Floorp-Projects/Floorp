// |jit-test| error:TypeError

// Binary: cache/js-dbg-64-ff51ddfdf5d1-linux
// Flags:
//
var a = [];
for (var i = 0; i < 200; ++i) a.push({});
var p = new Proxy({}, {preventExtensions() { return false; }});
Object.preventExtensions(p);
