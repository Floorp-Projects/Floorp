for (let i = 0, j = 0; i < 100; ++i) {
  let x = (-0xffffffffffffffff_ffffffffffffffffn >> 0x40n);
  assertEq(x, -0x10000000000000000n);
}
