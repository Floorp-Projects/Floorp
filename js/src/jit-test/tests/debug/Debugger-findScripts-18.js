// In a debuggee with multiple scripts with varying displayURLs (aka //#
// sourceURL), findScripts can filter by displayURL.

var g = newGlobal();

g.eval("function f(){} //# sourceURL=f.js");
g.eval("function g(){} //# sourceURL=g.js");
g.eval("function h(){}");

var dbg = new Debugger();
var gw = dbg.addDebuggee(g);
var fw = gw.makeDebuggeeValue(g.f);
var ggw = gw.makeDebuggeeValue(g.g);
var hw = gw.makeDebuggeeValue(g.h);

var fScripts = dbg.findScripts({ displayURL: "f.js" });
assertEq(fScripts.indexOf(fw.script) != -1, true);
assertEq(fScripts.indexOf(ggw.script), -1);
assertEq(fScripts.indexOf(hw.script), -1);


fScripts = dbg.findScripts({ displayURL: "f.js",
                             line: 1 });
assertEq(fScripts.indexOf(fw.script) != -1, true);
assertEq(fScripts.indexOf(ggw.script), -1);
assertEq(fScripts.indexOf(hw.script), -1);

var gScripts = dbg.findScripts({ displayURL: "g.js" });
assertEq(gScripts.indexOf(ggw.script) != -1, true);
assertEq(gScripts.indexOf(fw.script), -1);
assertEq(gScripts.indexOf(hw.script), -1);

var allScripts = dbg.findScripts();
assertEq(allScripts.indexOf(fw.script) != -1, true);
assertEq(allScripts.indexOf(ggw.script) != -1, true);
assertEq(allScripts.indexOf(hw.script) != -1, true);

try {
  dbg.findScripts({ displayURL: 3 });
  // Should never get here because the above line should throw
  // JSMSG_UNEXPECTED_TYPE.
  assertEq(true, false);
} catch(e) {
  assertEq(e.name, "TypeError");
  assertEq(e.message.contains("displayURL"), true);
}
