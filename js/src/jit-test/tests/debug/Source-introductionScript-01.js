// Dynamically generated sources should have their introduction script and
// offset set correctly.

var g = newGlobal();
var dbg = new Debugger;
var gDO = dbg.addDebuggee(g);
var log;

// Direct eval, while the frame is live.
dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  var source = frame.script.source;
  var introducer = frame.older;
  assertEq(source.introductionScript, introducer.script);
  assertEq(source.introductionOffset, introducer.offset);
};
log = '';
g.eval('\n\neval("\\n\\ndebugger;");');
assertEq(log, 'd');

// Direct eval, after the frame has been popped.
var introducer, introduced;
dbg.onDebuggerStatement = function (frame) {
  log += 'de1';
  introducer = frame.script;
  dbg.onDebuggerStatement = function (frame) {
    log += 'de2';
    introduced = frame.script.source;
  };
};
log = '';
g.evaluate('debugger; eval("\\n\\ndebugger;");', { lineNumber: 1812 });
assertEq(log, 'de1de2');
assertEq(introduced.introductionScript, introducer);
assertEq(introducer.getOffsetLocation(introduced.introductionOffset).lineNumber, 1812);

// Indirect eval, while the frame is live.
dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  var source = frame.script.source;
  var introducer = frame.older;
  assertEq(source.introductionScript, introducer.script);
  assertEq(source.introductionOffset, introducer.offset);
};
log = '';
g.eval('\n\n(0,eval)("\\n\\ndebugger;");');
assertEq(log, 'd');

// Indirect eval, after the frame has been popped.
var introducer, introduced;
dbg.onDebuggerStatement = function (frame) {
  log += 'de1';
  introducer = frame.script;
  dbg.onDebuggerStatement = function (frame) {
    log += 'de2';
    introduced = frame.script.source;
  };
};
log = '';
g.evaluate('debugger; (0,eval)("\\n\\ndebugger;");', { lineNumber: 1066 });
assertEq(log, 'de1de2');
assertEq(introduced.introductionScript, introducer);
assertEq(introducer.getOffsetLocation(introduced.introductionOffset).lineNumber, 1066);

// Function constructor.
dbg.onDebuggerStatement = function (frame) {
  log += 'o';
  var outerScript = frame.script;
  var outerOffset = frame.offset;
  dbg.onDebuggerStatement = function (frame) {
    log += 'i';
    var source = frame.script.source;
    assertEq(source.introductionScript, outerScript);
    assertEq(outerScript.getOffsetLocation(source.introductionOffset).lineNumber,
             outerScript.getOffsetLocation(outerOffset).lineNumber);
  };
};
log = '';
g.eval('\n\n\ndebugger; Function("debugger;")()');
assertEq(log, 'oi');

// Function constructor, after the the introduction call's frame has been
// popped.
var introducer;
dbg.onDebuggerStatement = function (frame) {
  log += 'F2';
  introducer = frame.script;
};
log = '';
var fDO = gDO.executeInGlobal('debugger; Function("origami;")', { lineNumber: 1685 }).return;
var source = fDO.script.source;
assertEq(log, 'F2');
assertEq(source.introductionScript, introducer);
assertEq(introducer.getOffsetLocation(source.introductionOffset).lineNumber, 1685);

// If the introduction script is in a different global from the script it
// introduced, we don't record it.
dbg.onDebuggerStatement = function (frame) {
  log += 'x';
  var source = frame.script.source;
  assertEq(source.introductionScript, undefined);
  assertEq(source.introductionOffset, undefined);
};
log = '';
g.eval('debugger;'); // introduction script is this top-level script
assertEq(log, 'x');

// If the code is introduced by a function that doesn't provide
// introduction information, that shouldn't be a problem.
dbg.onDebuggerStatement = function (frame) {
  log += 'x';
  var source = frame.script.source;
  assertEq(source.introductionScript, undefined);
  assertEq(source.introductionOffset, undefined);
};
log = '';
g.eval('evaluate("debugger;", { lineNumber: 1729 });');
assertEq(log, 'x');
