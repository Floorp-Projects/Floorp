var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
g.eval(`
var line0 = Error().lineNumber;
function f() {
    try {
  throw 4;
    } catch(e) {}
}
`);
var script = gw.getOwnPropertyDescriptor("f").value.script;
var handler = {
    hit: function() {}
};
var offs = script.getLineOffsets(g.line0 + 4);
for (var i = 0; i < offs.length; i++) script.setBreakpoint(offs[i], handler);
assertEq(g.f(), undefined);
