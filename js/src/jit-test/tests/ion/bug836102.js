function t() {
  var x = 0x123456789abcd;
  x = x + x; // x = 640511947003802
  x = x + x; // x = 1281023894007604
  x = x + x; // x = 2562047788015208
  x = x + x; // x = 5124095576030416
  x = x + x; // x = 10248191152060832
  assertEq(x+1 | 0, -248153696)
}
t()

function t2() {
  var x = -0x123456789abcd;
  x = x + x;
  x = x + x;
  x = x + x;
  x = x + x;
  x = x + x;
  assertEq(x + 7 | 0, 248153704)
}
t2()

function t() {
  var x = 4294967296+1;
  assertEq(x|0, 1);
}
