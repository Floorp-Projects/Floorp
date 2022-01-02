// obj.defineProperty throws if a value, getter, or setter is in a different compartment than obj

load(libdir + "asserts.js");
var g1 = newGlobal({newCompartment: true});
var g2 = newGlobal({newCompartment: true});
var dbg = new Debugger;
var g1w = dbg.addDebuggee(g1);
var g2w = dbg.addDebuggee(g2);
assertThrowsInstanceOf(function () { g1w.defineProperty('x', {value: g2w}); }, TypeError);
assertThrowsInstanceOf(function () { g1w.defineProperty('x', {get: g1w, set: g2w}); }, TypeError);
