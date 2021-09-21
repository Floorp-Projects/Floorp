// ExecuteInFrameScriptEnvironment shouldn't create yet another script.

var g = newGlobal({newCompartment: true});
var g2 = newGlobal({newCompartment: true});
var dbg = new Debugger(g, g2);
var log = '';
var canary = 42;

dbg.onNewScript = function (evalScript) {
  log += 'e';

  evalScript.setBreakpoint(0, {
    hit(frame) {
      log += 'b';
      assertEq(frame.script, evalScript);
    }
  });

  dbg.onNewScript = function (anotherScript) {
    log += '!';
  };
};

dbg.onDebuggerStatement = function (frame) {
  log += 'd';
};

assertEq(log, '');
var evalScope = g.evalReturningScope("canary = 'dead'; let lex = 42; debugger; // nee", g2);
assertEq(log, 'ebd');
assertEq(canary, 42);
assertEq(evalScope.canary, 'dead');
