// Bug 1239813: Don't let compartments get GC'd while findScripts is holding
// them in its ScriptQuery's hash set.

var g = newGlobal();
var dbg = new Debugger();
dbg.addDebuggee(g);
g = newGlobal({sameZoneAs: g.Math});
dbg.findScripts({get source() { gc(); return undefined; }});
