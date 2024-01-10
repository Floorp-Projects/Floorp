function test(half, minusOneHalf) {
  assertEq(10 - half, 9.5);
  assertEq(10 - minusOneHalf, 11.5);
  assertEq(half - 0, 0.5);
  assertEq(minusOneHalf - 1, -2.5);

  assertEq(10 * half, 5);
  assertEq(half * 10, 5);
  assertEq(10 * minusOneHalf, -15);
  assertEq(minusOneHalf * 10, -15);

  assertEq(10 / half, 20);
  assertEq(half / 1, 0.5);
  assertEq(12 / minusOneHalf, -8);
  assertEq(minusOneHalf / 1, -1.5);

  assertEq(10 % half, 0);
  assertEq(half % 1, 0.5);
  assertEq(12 % minusOneHalf, 0);
  assertEq(minusOneHalf % 1, -0.5);

  assertEq(10 ** half, Math.sqrt(10));
  assertEq(half ** 4, 0.0625);
  assertEq(16 ** minusOneHalf, 0.015625);
  assertEq(minusOneHalf ** 4, 5.0625);
}

for (var i = 0; i < 10; i++) {
  test(0.5, -1.5);
  test("0.5", "-1.5");
  test("5e-1", "-15e-1");
}
