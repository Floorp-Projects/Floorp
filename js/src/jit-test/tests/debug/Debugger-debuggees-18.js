// Debugger.prototype.{addDebuggee,hasDebuggee,removeDebuggee} recognize globals
// regardless of how they are specified.

var dbg = new Debugger;

// Assert that dbg's debuggees are exactly the set passed as arguments.
// The arguments are assumed to be Debugger.Object instances referring to
// globals without wrappers --- which is the sort returned by addDebuggee.
function assertDebuggees(...expected) {
  print("assertDebuggees([" + expected.map((g) => g.toSource()) + "])");
  var debuggees = dbg.getDebuggees();
  assertEq(expected.length, debuggees.length);
  for (let g of expected)
    assertEq(debuggees.indexOf(g) != -1, true);
}

var g1 = newGlobal(); g1.toSource = function () { return "[global g1]"; };
var g2 = newGlobal(); g2.toSource = function () { return "[global g2]"; };

assertDebuggees();

// Produce every possible way to designate g1, for us to play with.
// Globals can be designated by any of the following:
//
// - "CCW": a Cross-Compartment Wrapper (CCW) of a global object
// - "D.O": a Debugger.Object whose referent is a global object
// - "D.O of CCW": a Debugger.Object whose referent is a CCW of a
//   global object, where the CCW can be securely unwrapped
//
// There's no direct "G", since globals are always in their own
// compartments, never the debugger's; if we ever viewed them directly,
// that would be a compartment violation.

// "dg1" means "Debugger.Object referring (directly) to g1".
var dg1 = dbg.addDebuggee(g1);
dg1.toSource = function() { return "[Debugger.Object for global g1]"; };
assertEq(dg1.unwrap(), dg1);
assertDebuggees(dg1);

// We need to add g2 as a debuggee; that's the only way to get a D.O referring
// to it without a wrapper.
var dg2 = dbg.addDebuggee(g2);
dg2.toSource = function() { return "[Debugger.Object for global g2]"; };
assertEq(dg2.unwrap(), dg2);
assertDebuggees(dg1, dg2);

// "dwg1" means "Debugger.Object referring to CCW of g1".
var dwg1 = dg2.makeDebuggeeValue(g1);
assertEq(dwg1.unwrap(), dg1);
dwg1.toSource = function() { return "[Debugger.Object for CCW of global g1]"; };

assertDebuggees(dg1, dg2);
assertEq(dbg.removeDebuggee(g1), undefined);
assertEq(dbg.removeDebuggee(g2), undefined);
assertDebuggees();

// Systematically cover all the single-global possibilities:
//
//  | added as    | designated as | addDebuggee | hasDebuggee | removeDebuggee |
//  |-------------+---------------+-------------+-------------+----------------|
//  | (not added) | CCW           | X           | X           | X              |
//  |             | D.O           | X           | X           | X              |
//  |             | D.O of CCW    | X           | X           | X              |
//  |-------------+---------------+-------------+-------------+----------------|
//  | CCW         | CCW           | X           | X           | X              |
//  |             | D.O           | X           | X           | X              |
//  |             | D.O of CCW    | X           | X           | X              |
//  |-------------+---------------+-------------+-------------+----------------|
//  | D.O         | CCW           | X           | X           | X              |
//  |             | D.O           | X           | X           | X              |
//  |             | D.O of CCW    | X           | X           | X              |
//  |-------------+---------------+-------------+-------------+----------------|
//  | D.O of CCW  | CCW           | X           | X           | X              |
//  |             | D.O           | X           | X           | X              |
//  |             | D.O of CCW    | X           | X           | X              |

// Cover the "(not added)" section of the table, other than "addDebuggee":
assertEq(dbg.hasDebuggee(g1), false);
assertEq(dbg.hasDebuggee(dg1), false);
assertEq(dbg.hasDebuggee(dwg1), false);

assertEq(dbg.removeDebuggee(g1), undefined); assertDebuggees();
assertEq(dbg.removeDebuggee(dg1), undefined); assertDebuggees();
assertEq(dbg.removeDebuggee(dwg1), undefined); assertDebuggees();

// Try all operations adding the debuggee using |addAs|, and operating on it
// using |designateAs|, thereby covering one row of the table (outside the '(not
// added)' section), and one case in the '(not added)', 'designated as' section.
//
// |Direct| should be the Debugger.Object referring directly to the debuggee
// global, for checking the results from addDebuggee and getDebuggees.
function combo(addAs, designateAs, direct) {
  print("combo(" + uneval(addAs) + ", " + uneval(designateAs) + ")");
  assertDebuggees();
  assertEq(dbg.addDebuggee(addAs), direct);
  assertDebuggees(direct);
  assertEq(dbg.addDebuggee(designateAs), direct);
  assertDebuggees(direct);
  assertEq(dbg.hasDebuggee(designateAs), true);
  assertEq(dbg.removeDebuggee(designateAs), undefined);
  assertDebuggees();
}

combo(g1, g1, dg1);
combo(dg1, g1, dg1);
combo(dwg1, g1, dg1);

combo(g1, dg1, dg1);
combo(dg1, dg1, dg1);
combo(dwg1, dg1, dg1);

combo(g1, dwg1, dg1);
combo(dg1, dwg1, dg1);
combo(dwg1, dwg1, dg1);
