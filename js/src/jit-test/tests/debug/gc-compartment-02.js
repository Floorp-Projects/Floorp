// Referents of Debug.Objects in other compartments always survive per-compartment GC.

var g = newGlobal('new-compartment');
var dbg = Debug(g);
var arr = [];
dbg.hooks = {debuggerHandler: function (frame) { arr.push(frame.eval("[]").return); }};
g.eval("for (var i = 0; i < 10; i++) debugger;");
assertEq(arr.length, 10);

gc(g);

for (var i = 0; i < arr.length; i++)
    assertEq(arr[i].class, "Array");
