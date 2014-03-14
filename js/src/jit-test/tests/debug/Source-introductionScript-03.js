// We don't record introduction scripts in a different global from the
// introduced script, even if they're both debuggees.

var dbg = new Debugger;

var g1 = newGlobal();
g1.g1 = g1;
var g1DO = dbg.addDebuggee(g1);

var g2 = newGlobal();
g2.g1 = g1;

var log = '';
dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  assertEq(frame.script.source.introductionScript, undefined);
  assertEq(frame.script.source.introductionOffset, undefined);
};

g2.eval('g1.eval("debugger;");');
assertEq(log, 'd');

// Just for sanity: when it's not cross-global, we do note the introducer.
log = '';
dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  assertEq(frame.script.source.introductionScript instanceof Debugger.Script, true);
  assertEq(typeof frame.script.source.introductionOffset, "number");
};
// Exactly as above, but with g1 instead of g2.
g1.eval('g1.eval("debugger;");');
assertEq(log, 'd');
