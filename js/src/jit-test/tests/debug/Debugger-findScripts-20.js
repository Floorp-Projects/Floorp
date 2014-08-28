var g = newGlobal();
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);

g.eval('function f(){}');

var o = gw.makeDebuggeeValue(g.f);

var allScripts = dbg.findScripts();
var scripts = dbg.findScripts({
  source: o.script.source
});
assertEq(scripts.length < allScripts.length, true);
assertEq(scripts.indexOf(o.script) !== -1, true);

scripts = dbg.findScripts({
  source: o.script.source,
  line: 1
});
assertEq(scripts.indexOf(o.script) !== -1, true);
