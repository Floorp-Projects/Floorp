// Check that scripts' introduction types are properly marked.

var g = newGlobal();
var dbg = new Debugger();
var gDO = dbg.addDebuggee(g);
var log;

// (Indirect) eval.
dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  assertEq(frame.script.source.introductionType, 'eval');
};
log = '';
g.eval('debugger;');
assertEq(log, 'd');

// Function constructor.
dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  assertEq(frame.script.source.introductionType, 'Function');
};
log = '';
g.Function('debugger;')();
assertEq(log, 'd');

// GeneratorFunction constructor.
dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  assertEq(frame.script.source.introductionType, 'GeneratorFunction');
};
log = '';
g.eval('(function*() {})').constructor('debugger;')().next();
assertEq(log, 'd');

// Shell 'evaluate' function
dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  assertEq(frame.script.source.introductionType, "js shell evaluate");
};
log = '';
g.evaluate('debugger;');
assertEq(log, 'd');

// Shell 'load' function
dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  assertEq(frame.script.source.introductionType, "js shell load");
};
log = '';
g.load(scriptdir + 'Source-introductionType-data');
assertEq(log, 'd');

// Shell 'run' function
dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  assertEq(frame.script.source.introductionType, "js shell run");
};
log = '';
g.run(scriptdir + 'Source-introductionType-data');
assertEq(log, 'd');

// Shell 'offThreadCompileScript' function.
dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  assertEq(frame.script.source.introductionType, "js shell offThreadCompileScript");
};
log = '';
g.offThreadCompileScript('debugger;');
g.runOffThreadScript();
assertEq(log, 'd');

// Debugger.Frame.prototype.eval
dbg.onDebuggerStatement = function (frame) {
  log += 'o';
  dbg.onDebuggerStatement = innerHandler;
  frame.eval('debugger');
  function innerHandler(frame) {
    log += 'i';
    assertEq(frame.script.source.introductionType, "debugger eval");
  }
};
log = '';
g.eval('debugger;');
assertEq(log, 'oi');

// Debugger.Frame.prototype.evalWithBindings
dbg.onDebuggerStatement = function (frame) {
  log += 'o';
  dbg.onDebuggerStatement = innerHandler;
  frame.evalWithBindings('debugger', { x: 42 });
  function innerHandler(frame) {
    log += 'i';
    assertEq(frame.script.source.introductionType, "debugger eval");
  }
};
log = '';
g.eval('debugger;');
assertEq(log, 'oi');

// Debugger.Object.evalInGlobal
dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  assertEq(frame.script.source.introductionType, "debugger eval");
};
log = '';
gDO.evalInGlobal('debugger;');
assertEq(log, 'd');

// Debugger.Object.evalInGlobalWithBindings
dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  assertEq(frame.script.source.introductionType, "debugger eval");
};
log = '';
gDO.evalInGlobalWithBindings('debugger;', { x: 42 });
assertEq(log, 'd');

