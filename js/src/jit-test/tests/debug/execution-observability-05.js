// Test that we can do debug mode OSR from the interrupt handler through an
// on->off->on cycle.

var global = this;
var hits = 0;
setInterruptCallback(function() {
  print("Interrupt!");
  hits++;
  var g = newGlobal();
  g.parent = global;
  g.eval("var dbg = new Debugger(parent); dbg.onEnterFrame = function(frame) {};");
  g.eval("dbg.removeDebuggee(parent);");
  return true;
});

function f(x) {
  if (x > 200)
    return;
  interruptIf(x == 100);
  f(x + 1);
}
f(0);
assertEq(hits, 1);
