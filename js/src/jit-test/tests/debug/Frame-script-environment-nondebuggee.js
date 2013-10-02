// The script and environment of a non-debuggee frame are inaccessible.

load(libdir + 'asserts.js');

var g = newGlobal();
var dbg = new Debugger;

var log;
dbg.onDebuggerStatement = function (frame) {
  log += frame.type;
  // Initially, 'frame' is a debuggee frame, and we should be able to see its script and environment.
  assertEq(frame.script instanceof Debugger.Script, true);
  assertEq(frame.environment instanceof Debugger.Environment, true);

  // If we make g no longer a debuggee, then trying to touch the frame at
  // all show throw.
  dbg.removeDebuggee(g);
  assertThrowsInstanceOf(() => frame.script, Error);
  assertThrowsInstanceOf(() => frame.environment, Error);
}

g.eval('function f() { debugger; }');

log = '';
dbg.addDebuggee(g);
g.f();
assertEq(log, 'call');

log = '';
dbg.addDebuggee(g);
g.eval('debugger;');
assertEq(log, 'eval');
