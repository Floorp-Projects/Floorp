function returnZero() { return 0; } 
function test() {
  var a = "a";
  var b = "b";
  if (returnZero()) {
    return a + b;
  } else {
    return b + a;
  }
}
assertEq(test(), "ba");

