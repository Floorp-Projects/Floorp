
/* Handle recompilation on non-inc/dec arithmetic operations. */

function add(x, y)
{
  var z = x + y;
  assertEq(z, 2147483732);
  assertEq(z - 10, 2147483722);
}
add(0x7ffffff0, 100);

function mul(x, y)
{
  var z = x * y;
  assertEq(z, 4294967264);
}
mul(0x7ffffff0, 2);

function div1(x, y)
{
  var z = x / y;
  assertEq(z + 10, 20);
}
div1(100, 10);

function div2(x, y)
{
  var z = x / y;
  assertEq(z + 10, 20.5);
}
div2(105, 10);
