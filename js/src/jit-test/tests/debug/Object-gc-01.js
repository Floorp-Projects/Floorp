// Debugger.Objects keep their referents alive.

var g = newGlobal();
var dbg = Debugger(g);
var arr = [];
dbg.onDebuggerStatement = function (frame) { arr.push(frame.eval("[]").return); };
g.eval("for (var i = 0; i < 10; i++) debugger;");
assertEq(arr.length, 10);

gc();

for (var i = 0; i < arr.length; i++)
    assertEq(arr[i].class, "Array");

