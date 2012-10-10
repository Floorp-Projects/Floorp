// var statements in non-strict evalWithBindings code behave like non-strict direct eval.
var g = newGlobal();
var dbg = new Debugger(g);
var log;
dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  assertEq(frame.evalWithBindings("var i = v; 42;", { v: 'inner' }).return, 42);
};

g.i = 'outer';
log = '';
assertEq(g.eval('debugger; i;'), 'inner');
assertEq(log, 'd');

g.j = 'outer';
log = '';
assertEq(g.eval('debugger; j;'), 'outer');
assertEq(log, 'd');
