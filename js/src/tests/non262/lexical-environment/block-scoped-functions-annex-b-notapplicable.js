// Test that functions in block that do not exhibit Annex B do not override
// previous functions that do exhibit Annex B.

function f() {
  var outerX;
  { function x() {1} outerX = x; }
  { { function x() {2}; } let x; }
  { let x; { function x() {3}; } }
  assertEq(x, outerX);
}
f();

if (typeof reportCompare === "function")
  reportCompare(0, 0);
