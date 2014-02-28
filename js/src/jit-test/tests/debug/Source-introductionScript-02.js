// Calls to 'eval', etc. by JS primitives get attributed to the point of
// the primitive's call.

var g = newGlobal();
var dbg = new Debugger;
var gDO = dbg.addDebuggee(g);
var log = '';

function outerHandler(frame) {
  log += 'o';
  var outerScript = frame.script;

  dbg.onDebuggerStatement = function (frame) {
    log += 'i';
    var source = frame.script.source;
    var introScript = source.introductionScript;
    assertEq(introScript, outerScript);
    assertEq(introScript.getOffsetLine(source.introductionOffset), 1234);
  };
};

log = '';
dbg.onDebuggerStatement = outerHandler;
g.evaluate('debugger; ["debugger;"].map(eval)', { lineNumber: 1234 });
assertEq(log, 'oi');

log = '';
dbg.onDebuggerStatement = outerHandler;
g.evaluate('debugger; "debugger;".replace(/.*/, eval);',
           { lineNumber: 1234 });
assertEq(log, 'oi');
