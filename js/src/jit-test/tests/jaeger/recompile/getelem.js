
/* Unexpected values out of GETELEM */

function foo() {
  var x = [1,2,3];
  var y;
  var z = x[y];
  y = 10;
  assertEq(z, "twelve");
}
Array.prototype["undefined"] = "twelve";
foo();
