// In a debuggee, the Function.prototype script source has the introductionType
// property set to "Function.prototype".

var g = newGlobal();
var dbg = new Debugger(g);
var scripts = dbg.findScripts();
assertEq(scripts.length > 0, true);
var fpScripts = scripts.filter(s => s.source.introductionType == "Function.prototype");
assertEq(fpScripts.length, 1);
var source = fpScripts[0].source;
assertEq(source.text, "function () {\n}");
