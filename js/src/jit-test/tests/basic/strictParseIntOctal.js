"use strict";

assertEq(parseInt("08"), 8);
assertEq(parseInt("09"), 9);
assertEq(parseInt("014"), 14);
assertEq(parseInt("0xA"), 10);
assertEq(parseInt("00123"), 123);

for (var i = 0; i < 5; i++)
{
  assertEq(parseInt("08"), 8);
  assertEq(parseInt("09"), 9);
  assertEq(parseInt("014"), 14);
  assertEq(parseInt("0xA"), 10);
  assertEq(parseInt("00123"), 123);
}
