
/* Test undoing addition in overflow paths when under heavy register pressure. */

function add1(x, y, a, b, res) { var nres = res + 0; var z = (x + a) + (y + b); assertEq(z, nres); }
function add2(x, y, a, b, res) { var nres = res + 0; var z = (x + a) + (y + b); assertEq(z, nres); }
function add3(x, y, a, b, res) { var nres = res + 0; var z = (x + a) + (y + b); assertEq(z, nres); }
add1(0x7ffffff0, 100, 0, 0, 2147483732);
add2(-1000, -0x80000000, 0, 0, -2147484648);
add3(-0x80000000, -1000, 0, 0, -2147484648);

function cadd1(x, a, b, res) {
  var nres = res + 0;
  var nb = b + 0;
  var z = (x + a) + 1000;
  assertEq(z, nres + nb);
}
cadd1(0x7ffffff0, 0, 0, 2147484632);

function cadd2(x, a, b, res) {
  var nres = res + 0;
  var nb = b + 0;
  var z = (x + a) + (-0x80000000);
  assertEq(z, nres + nb);
}
cadd2(-1000, 0, 0, -2147484648);
