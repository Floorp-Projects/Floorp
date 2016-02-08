
var g = newGlobal();
var dbg = new Debugger(g);
g.eval("" + function f(c) {
  if (c == 0)
    return;
  if (c == 2)
    debugger;
  f(c-1);
  for (var i = 0; i < 100; i++)
    Debugger += newGlobal('#15: myObj.parseFloat !== parseFloat');
});
dbg.onDebuggerStatement = function (frame) {};
g.eval("f(2)");
