// |jit-test| --fuzzing-safe
// Debugger.findObjects returns an empty array with --fuzzing-safe

var g = newGlobal({newCompartment: true});
g.evaluate("arr = [1, 2, 3].map(x => x + 1)");
var dbg = new Debugger(g);
assertEq(dbg.findObjects().length, 0);
