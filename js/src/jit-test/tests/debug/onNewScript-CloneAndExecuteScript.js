// Debugger should be notified of scripts created with cloneAndExecuteScript.

var g = newGlobal({newCompartment: true});
var g2 = newGlobal({newCompartment: true});
var dbg = new Debugger(g, g2);
var log = '';

dbg.onNewScript = function (evalScript) {
  log += 'e';

  dbg.onNewScript = function (clonedScript) {
    log += 'c';
    clonedScript.setBreakpoint(0, {
      hit(frame) {
        log += 'b';
        assertEq(frame.script, clonedScript);
      }
    });
  };
};

dbg.onDebuggerStatement = function (frame) {
  log += 'd';
};

assertEq(log, '');
g.cloneAndExecuteScript("debugger; // nee", g2);
assertEq(log, 'ecbd');
