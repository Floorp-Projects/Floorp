// Test that Annex B function interaction with 'arguments'.

(function() {
  assertEq(typeof arguments, "object");
  { function arguments() {} }
  assertEq(typeof arguments, "function");
})();

if (typeof reportCompare === "function")
  reportCompare(0, 0);
