var x = 0b1111n << 126n;
// octets:     g f e d c b a 9 8 7 6 5 4 3 2 1 0
assertEq(x, 0x03c0000000000000000000000000000000n);

function tr(bits, x) { return BigInt.asUintN(bits, x); }

// octets:              g f e d c b a 9 8 7 6 5 4 3 2 1 0
assertEq(tr(131, x), 0x03c0000000000000000000000000000000n);
assertEq(tr(130, x), 0x03c0000000000000000000000000000000n);
assertEq(tr(129, x), 0x01c0000000000000000000000000000000n);
assertEq(tr(128, x), 0x00c0000000000000000000000000000000n);
assertEq(tr(127, x), 0x0040000000000000000000000000000000n);
assertEq(tr(126, x), 0n);
