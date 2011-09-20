// Referents of Debugger.Scripts in other compartments always survive per-compartment GC.

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
var arr = [];
dbg.onDebuggerStatement = function (frame) { arr.push(frame.script); };
g.eval("for (var i = 0; i < 100; i++) Function('debugger;')();");
assertEq(arr.length, 100);

gc(g);

for (var i = 0; i < arr.length; i++)
    assertEq(arr[i].lineCount, 1);

gc();
