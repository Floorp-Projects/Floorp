// Check that scripts' introduction types are properly marked.

var g = newGlobal();
var dbg = new Debugger(g);
var log;

dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  assertEq(frame.script.source.introductionType, 'eval');
};
log = '';
g.eval('debugger;');
assertEq(log, 'd');

dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  assertEq(frame.script.source.introductionType, 'Function');
};
log = '';
g.Function('debugger;')();
assertEq(log, 'd');

dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  assertEq(frame.script.source.introductionType, 'GeneratorFunction');
};
log = '';
g.eval('(function*() {})').constructor('debugger;')().next();
assertEq(log, 'd');

dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  assertEq(frame.script.source.introductionType, undefined);
};
log = '';
g.evaluate('debugger;');
assertEq(log, 'd');

