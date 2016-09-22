var g = newGlobal();
var dbg = new Debugger(g);

dbg.onDebuggerStatement = function (frame) {
  // The bindings object is unused but adds another environment on the
  // environment chain. Make sure 'this' computes the right value in light of
  // this.
  assertEq(frame.evalWithBindings(`this === foo;`, { bar: 42 }).return, true);
  assertEq(frame.evalWithBindings(`eval('this') === foo;`, { bar: 42 }).return, true);
};

g.eval(`
var foo = { bar: function() { debugger; } };
foo.bar();
`);
