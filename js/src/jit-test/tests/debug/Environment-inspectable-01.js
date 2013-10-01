// Environments are only inspectable while their globals are debuggees.

load(libdir + 'asserts.js');

var g1 = newGlobal();
var g2 = newGlobal();
g2.g1 = g1;
g1.g2 = g2;

g1.eval('function f(xf) { return function h(xh) { debugger; } }');
g1.eval('var h = f("value of xf");');

// To ensure that xk gets located on the heap, and thus outlives its stack frame, we
// store a function that captures it here. Kind of a kludge.
g2.eval('var capture;');
g2.eval('function k(xk) { capture = function () { return xk; }; g1.h("value of xh"); }');

var dbg = new Debugger;
dbg.addDebuggee(g1);
dbg.addDebuggee(g2);

dbg.onDebuggerStatement = debuggerHandler;

var log = '';

g1.eval('g2.k("value of xk");');

var he, ke, ee;

function debuggerHandler(frame) {
  log += 'd';

  assertEq(frame.type, 'call');
  he = frame.environment;

  assertEq(frame.older.type, 'call');
  ke = frame.older.environment;

  assertEq(frame.older.older.type, 'eval');
  ee = frame.older.older.environment;

  assertEq(he.inspectable, true);
  assertEq(he.getVariable('xh'), 'value of xh');
  assertEq(he.parent.parent.getVariable('xf'), 'value of xf');
  assertEq(ke.inspectable, true);
  assertEq(ke.getVariable('xk'), 'value of xk');
  assertEq(ee.inspectable, true);
  assertEq(ee.type, 'object');

  dbg.removeDebuggee(g2);

  assertEq(he.inspectable, true);
  assertEq(he.type, 'declarative');
  assertEq(ke.inspectable, false);
  assertThrowsInstanceOf(() => ke.getVariable('xk'), Error);
  assertEq(ee.inspectable, true);
  assertEq(ee.type, 'object');

  dbg.removeDebuggee(g1);

  assertEq(he.inspectable, false);
  assertThrowsInstanceOf(() => he.getVariable('xh'), Error);
  assertEq(ke.inspectable, false);
  assertThrowsInstanceOf(() => ke.getVariable('xk'), Error);
  assertEq(ee.inspectable, false);
  assertThrowsInstanceOf(() => ee.type, Error);
}

assertEq(log, 'd');

dbg.addDebuggee(g2);

assertEq(he.inspectable, false);
assertThrowsInstanceOf(() => he.getVariable('xh'), Error);
assertEq(ke.inspectable, true);
assertEq(ke.getVariable('xk'), 'value of xk');
assertEq(ee.inspectable, false);
assertThrowsInstanceOf(() => ee.type, Error);
