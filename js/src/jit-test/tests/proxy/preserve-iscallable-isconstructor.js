load(libdir + "asserts.js");

var global = newGlobal()
var fun = global.eval("(function() {})")
var p = new Proxy(fun, {})

// Nuking should preserve IsCallable and IsConstructor.
assertEq(isConstructor(p), true);
assertEq(typeof p, "function");
nukeCCW(fun);
assertEq(isConstructor(p), true);
assertEq(typeof p, "function");

// But actually calling and constructing should throw, because it's been
// nuked.
assertThrowsInstanceOf(() => { p(); }, TypeError);
assertThrowsInstanceOf(() => { new p(); }, TypeError);
