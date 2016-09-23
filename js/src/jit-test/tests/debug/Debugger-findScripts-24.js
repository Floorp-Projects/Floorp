// findScripts should reject Debugger.Source objects from other Debuggers.

load(libdir + 'asserts.js');

var g = newGlobal();
g.evaluate(`function f() { print("earth/heart/hater"); }`,
           { lineNumber: 1800 });

var dbg1 = new Debugger;
var gDO1 = dbg1.addDebuggee(g);
var fDO1 = gDO1.getOwnPropertyDescriptor('f').value;
assertEq(fDO1.script.source instanceof Debugger.Source, true);

var dbg2 = new Debugger;
var gDO2 = dbg2.addDebuggee(g);
var fDO2 = gDO2.getOwnPropertyDescriptor('f').value;
assertEq(fDO2.script.source instanceof Debugger.Source, true);

assertEq(fDO1.script.source !== fDO2.script.source, true);

// findScripts should reject Debugger.Source objects that don't belong to the
// Debugger it's being invoked on.
assertThrowsInstanceOf(() => dbg1.findScripts({ source: fDO2.script.source }),
                       TypeError);
assertThrowsInstanceOf(() => dbg2.findScripts({ source: fDO1.script.source }),
                       TypeError);

// findScripts should reject Debugger.Source.prototype.
assertThrowsInstanceOf(() => dbg1.findScripts({ source: Debugger.Source.prototype }),
                       TypeError);

// These should not throw, and should return something, but we're not going to
// be picky about exactly what, given findScript's sensitivity to GC.
dbg1.findScripts({ source: fDO1.script.source });
dbg2.findScripts({ source: fDO2.script.source });
