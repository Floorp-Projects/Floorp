function arity1(x)
{
  return (x == undefined) ? 1 : 0;
}
function missingArgTest() {
  var q;
  for (var i = 0; i < 10; i++) {
    q = arity1();
  }
  return q;
}
assertEq(missingArgTest(), 1);
