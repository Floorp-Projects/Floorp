// Debugger.Object.prototype.defineProperty with too few arguments throws.

load(libdir + "asserts.js");

var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
assertThrowsInstanceOf(function () { gw.defineProperty("x"); }, TypeError);
