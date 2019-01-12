// In a debugger with scripts, findSources finds the script source.

const g = newGlobal({newCompartment: true});
// Declare a function in order to keep the script source alive across GC.
g.evaluate(`function fa() {}`, { fileName: "a.js" });
g.evaluate(`function fb() {}`, { fileName: "b.js" });
g.evaluate(`function fc() {}`, { fileName: "c.js" });

const dbg = new Debugger();
const gw = dbg.addDebuggee(g);

const sources = dbg.findSources();
assertEq(sources.filter(s => s.url == "a.js").length, 1);
assertEq(sources.filter(s => s.url == "b.js").length, 1);
assertEq(sources.filter(s => s.url == "c.js").length, 1);
