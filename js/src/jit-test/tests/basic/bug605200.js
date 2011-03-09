
function foo() {
  var name = 2;
  var x = <a><name>what</name></a>;
  var y = "" + x.(name != 2);
  assertEq(y.match(/what/)[0], "what");
  var z = "" + x.(name = 10);
  assertEq(z.match(/10/)[0], "10");
}
foo();
