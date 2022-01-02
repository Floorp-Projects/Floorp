
/* Element initializers with unknown index. */

function foo(i) {
  var x = [1,2,i == 1 ? 3 : 4,5,6];
  var y = "" + x;
  if (i == 1)
    assertEq(y, "1,2,3,5,6");
  else
    assertEq(y, "1,2,4,5,6");
}
for (var i = 0; i < 10; i++)
  foo(i);
