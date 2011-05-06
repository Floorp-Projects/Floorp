// |jit-test| debug
// simplest possible test of Debug.Frame.prototype.eval

var g = newGlobal('new-compartment');
var dbg = new Debug(g);
var c;
dbg.hooks = {debuggerHandler: function (frame) { c = frame.eval("2 + 2"); }};
g.eval("debugger;");
assertEq(c.return, 4);
