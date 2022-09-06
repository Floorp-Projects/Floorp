// |jit-test| skip-if: helperThreadCount() === 0

// Check that scripts' introduction types are properly marked.

var g = newGlobal({newCompartment: true});
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

// Shell 'offThreadCompileToStencil' function.
dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  assertEq(frame.script.source.introductionType,
           "js shell offThreadCompileToStencil");
};
log = '';
g.offThreadCompileToStencil('debugger;');
var stencil = g.finishOffThreadStencil();
g.evalStencil(stencil);
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

// Debugger.Object.executeInGlobal
dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  assertEq(frame.script.source.introductionType, "debugger eval");
};
log = '';
gDO.executeInGlobal('debugger;');
assertEq(log, 'd');

// Debugger.Object.executeInGlobalWithBindings
dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  assertEq(frame.script.source.introductionType, "debugger eval");
};
log = '';
gDO.executeInGlobalWithBindings('debugger;', { x: 42 });
assertEq(log, 'd');

