// Debugger.Object.prototype.executeInGlobal argument validation

load(libdir + 'asserts.js');

var g = newGlobal();
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);
var gobj = gw.makeDebuggeeValue(g.eval("({})"));

assertThrowsInstanceOf(function () { gw.executeInGlobal(); }, TypeError);
assertThrowsInstanceOf(function () { gw.executeInGlobal(10); }, TypeError);
assertThrowsInstanceOf(function () { gobj.executeInGlobal('42'); }, TypeError);
assertEq(gw.executeInGlobal('42').return, 42);

assertThrowsInstanceOf(function () { gw.executeInGlobalWithBindings(); }, TypeError);
assertThrowsInstanceOf(function () { gw.executeInGlobalWithBindings('42'); }, TypeError);
assertThrowsInstanceOf(function () { gw.executeInGlobalWithBindings(10, 1729); }, TypeError);
assertThrowsInstanceOf(function () { gw.executeInGlobalWithBindings('42', 1729); }, TypeError);
assertThrowsInstanceOf(function () { gobj.executeInGlobalWithBindings('42', {}); }, TypeError);
assertEq(gw.executeInGlobalWithBindings('42', {}).return, 42);
