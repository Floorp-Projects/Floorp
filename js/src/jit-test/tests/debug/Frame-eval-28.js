// Test that strict Debugger.Frame.eval has a correct static scope.
"use strict";
var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);
var hits = 0;
dbg.onEnterFrame = function(f) {
  hits++;
  if (hits > 2)
    return;
  f.eval("42");
};
g.eval("42");
