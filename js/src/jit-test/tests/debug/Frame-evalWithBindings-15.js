var g = newGlobal();
var dbg = new Debugger(g);

dbg.onDebuggerStatement = function (frame) {
  // The bindings object is unused but adds another environment on the
  // environment chain. Make sure 'this' computes the right value in light of
  // this.
  frame.evalWithBindings(`assertEq(this, foo);`, { bar: 42 });
};

g.eval(`
var foo = { bar: function() { debugger; } };
foo.bar();
`);
