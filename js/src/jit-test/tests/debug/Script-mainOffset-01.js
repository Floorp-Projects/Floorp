/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
// The main offset of a script should be hit before it performs any actions.

var g = newGlobal();
g.eval("var n = 0; function foo() { n = 1; }");
var dbg = Debugger(g);

var hits = 0;
function breakpointHit(frame) {
  hits++;
  assertEq(frame.eval("n").return, 0);
}

dbg.onDebuggerStatement = function (frame) {
  var script = frame.eval("foo").return.script;
  script.setBreakpoint(script.mainOffset, { hit: breakpointHit });
};
g.eval("debugger; foo()");
assertEq(g.eval("n"), 1);
assertEq(hits, 1);
