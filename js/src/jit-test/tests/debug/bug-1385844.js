// Frame invalidatation should not follow 'debugger eval prev' links.
// This should not fail a 'frame.isDebuggee()' assertion.

var g1 = this;

var h = newGlobal({ newCompartment: true });
h.parent = g1;
h.eval(`
  var hdbg = new Debugger(parent);
  function j() {
    hdbg.onEnterFrame = function(frame) {};
  }
`);

var g2 = newGlobal({ newCompartment: true });
g2.j = h.j;

var dbg = new Debugger(g2);
var g2DO = dbg.addDebuggee(g2);

dbg.onDebuggerStatement = function(f) {
  f.eval('j()');
};
g2.eval('debugger');
