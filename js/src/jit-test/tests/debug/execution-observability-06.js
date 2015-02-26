// Test that OSR respect debuggeeness.

var g = newGlobal();
var dbg = new Debugger(g);

g.eval("" + function f(c) {
  if (c == 0)
    return;
  if (c == 2)
    debugger;
  f(c-1);
  acc = 0;
  for (var i = 0; i < 100; i++)
    acc += i;
});

var log = "";
dbg.onDebuggerStatement = function (frame) {
  frame.onPop = function f() { log += "p"; }
};

g.eval("f(2)");

assertEq(log, "p");
