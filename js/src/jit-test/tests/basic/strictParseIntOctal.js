"use strict";

assertEq(parseInt("08"), 0);
assertEq(parseInt("09"), 0);
assertEq(parseInt("014"), 12);
assertEq(parseInt("0xA"), 10);
assertEq(parseInt("00123"), 83);

for (var i = 0; i < 5; i++)
{
  assertEq(parseInt("08"), 0);
  assertEq(parseInt("09"), 0);
  assertEq(parseInt("014"), 12);
  assertEq(parseInt("0xA"), 10);
  assertEq(parseInt("00123"), 83);
}
