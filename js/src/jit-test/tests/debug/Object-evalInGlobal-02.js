// Debugger.Object.prototype.evalInGlobal argument validation

load(libdir + 'asserts.js');

var g = newGlobal();
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);
var gobj = gw.makeDebuggeeValue(g.eval("({})"));

assertThrowsInstanceOf(function () { gw.evalInGlobal(); }, TypeError);
assertThrowsInstanceOf(function () { gw.evalInGlobal(10); }, TypeError);
assertThrowsInstanceOf(function () { gobj.evalInGlobal('42'); }, TypeError);
assertEq(gw.evalInGlobal('42').return, 42);

assertThrowsInstanceOf(function () { gw.evalInGlobalWithBindings(); }, TypeError);
assertThrowsInstanceOf(function () { gw.evalInGlobalWithBindings('42'); }, TypeError);
assertThrowsInstanceOf(function () { gw.evalInGlobalWithBindings(10, 1729); }, TypeError);
assertThrowsInstanceOf(function () { gw.evalInGlobalWithBindings('42', 1729); }, TypeError);
assertThrowsInstanceOf(function () { gobj.evalInGlobalWithBindings('42', {}); }, TypeError);
assertEq(gw.evalInGlobalWithBindings('42', {}).return, 42);
