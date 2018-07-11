// Debugger.Script.prototype.script returns the global the script runs in.
// Multi-global version.

var dbg = new Debugger;

var g1 = newGlobal();
var g1w = dbg.addDebuggee(g1);

var g2 = newGlobal();
var g2w = dbg.addDebuggee(g2);

var g3 = newGlobal();
var g3w = dbg.addDebuggee(g3);

var log = '';
dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  assertEq(frame.script.global, g1w);
  assertEq(frame.older.script.global, g2w);
  assertEq(frame.older.older.script.global, g3w);
  assertEq(frame.older.older.older.script.global, g1w);
}

g1.eval('function f() { debugger; }');

g2.g1 = g1;
g2.eval('function g() { g1.f(); }');

g3.g2 = g2;
g3.eval('function h() { g2.g(); }');

g1.g3 = g3;
g1.eval('function i() { g3.h(); }');

g1.i();
assertEq(log, 'd');
