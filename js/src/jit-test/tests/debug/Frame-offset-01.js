// frame.offset throws if !frame.live.

load(libdir + "asserts.js");

var g = newGlobal('new-compartment');
var dbg = Debug(g);
var f;
dbg.hooks = {debuggerHandler: function (frame) { f = frame; }};
g.eval("debugger;");
assertEq(f.live, false);
assertThrowsInstanceOf(function () { f.offset; }, Error);
