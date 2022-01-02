// Test that we can do debug mode OSR from the interrupt handler.

var global = this;
var hits = 0;
setInterruptCallback(function() {
  print("Interrupt!");
  hits++;
  var g = newGlobal({newCompartment: true});
  g.parent = global;
  g.eval("var dbg = new Debugger(parent); dbg.onEnterFrame = function(frame) {};");
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
