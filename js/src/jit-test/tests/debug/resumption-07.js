// Return resumption values to non-debuggee frames.

load(libdir + 'asserts.js');

var g = newGlobal();
var dbg = new Debugger;

var log;

function handlerWithResumption(resumption) {
  return function (frame) {
    log += 'd';
    dbg.removeDebuggee(g);
    return resumption;
  };
}

log = '';
dbg.onDebuggerStatement = handlerWithResumption(undefined);
dbg.addDebuggee(g);
assertEq(g.eval('debugger; 42;'), 42);
assertEq(log, 'd');

log = '';
dbg.onDebuggerStatement = handlerWithResumption({ return: 1729 });
dbg.addDebuggee(g);
assertEq(g.eval('debugger; 42;'), 1729);
assertEq(log, 'd');

log = '';
dbg.onDebuggerStatement = handlerWithResumption(null);
dbg.addDebuggee(g);
assertEq(g.evaluate('debugger; 42;', { catchTermination: true }), 'terminated');
assertEq(log, 'd');
