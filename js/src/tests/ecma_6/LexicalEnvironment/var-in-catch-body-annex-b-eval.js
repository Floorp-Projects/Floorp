// Tests annex B.3.5 that introduces a var via direct eval.

var x = "global-x";
var log = "";

// Tests that direct eval works.
function g() {
  try { throw 8; } catch (x) {
    eval("var x = 42;");
    log += x;
  }
  x = "g";
  log += x;
}
g();

assertEq(x, "global-x");
assertEq(log, "42g");

if ("reportCompare" in this)
  reportCompare(true, true)
