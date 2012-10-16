// addDebuggee, hasDebuggee, and removeDebuggee all throw if presented with
// objects that are not valid global object designators.

load(libdir + 'asserts.js');

var dbg = new Debugger;

function check(bad) {
  print("check(" + uneval(bad) + ")");
  assertThrowsInstanceOf(function () { dbg.addDebuggee(bad); }, TypeError);
  assertEq(dbg.getDebuggees().length, 0);
  assertThrowsInstanceOf(function () { dbg.hasDebuggee(bad); }, TypeError);
  assertThrowsInstanceOf(function () { dbg.removeDebuggee(bad); }, TypeError);
}

var g = newGlobal();
check(g.Object());
check(g.Object);
check(g.Function(""));

// A Debugger.Object belonging to a different Debugger is not a valid way
// to designate a global, even if its referent is a global.
var g2 = newGlobal();
var dbg2 = new Debugger;
var d2g2 = dbg2.addDebuggee(g2);
check(d2g2);
