const tests = [
  [-0x10000000000000001n, 0xffffffffffffffffn],
  [-0x10000000000000000n, 0n],
  [-0xffffffffffffffffn, 1n],
  [-0xfffffffffffffffen, 2n],
  [-0x8000000000000001n, 0x7fffffffffffffffn],
  [-0x8000000000000000n, 0x8000000000000000n],
  [-0x7fffffffffffffffn, 0x8000000000000001n],
  [-0x7ffffffffffffffen, 0x8000000000000002n],
  [-0x100000001n, 0xfffffffeffffffffn],
  [-0x100000000n, 0xffffffff00000000n],
  [-0xffffffffn, 0xffffffff00000001n],
  [-0xfffffffen, 0xffffffff00000002n],
  [-0x80000001n, 0xffffffff7fffffffn],
  [-0x80000000n, 0xffffffff80000000n],
  [-0x7fffffffn, 0xffffffff80000001n],
  [-0x7ffffffen, 0xffffffff80000002n],
  [-9n, 0xfffffffffffffff7n],
  [-8n, 0xfffffffffffffff8n],
  [-7n, 0xfffffffffffffff9n],
  [-6n, 0xfffffffffffffffan],
  [-5n, 0xfffffffffffffffbn],
  [-4n, 0xfffffffffffffffcn],
  [-3n, 0xfffffffffffffffdn],
  [-2n, 0xfffffffffffffffen],
  [-1n, 0xffffffffffffffffn],
  [0n, 0n],
  [1n, 1n],
  [2n, 2n],
  [3n, 3n],
  [4n, 4n],
  [5n, 5n],
  [6n, 6n],
  [7n, 7n],
  [8n, 8n],
  [9n, 9n],
  [0x7ffffffen, 0x7ffffffen],
  [0x7fffffffn, 0x7fffffffn],
  [0x80000000n, 0x80000000n],
  [0x80000001n, 0x80000001n],
  [0xfffffffen, 0xfffffffen],
  [0xffffffffn, 0xffffffffn],
  [0x100000000n, 0x100000000n],
  [0x100000001n, 0x100000001n],
  [0x7ffffffffffffffen, 0x7ffffffffffffffen],
  [0x7fffffffffffffffn, 0x7fffffffffffffffn],
  [0x8000000000000000n, 0x8000000000000000n],
  [0x8000000000000001n, 0x8000000000000001n],
  [0xfffffffffffffffen, 0xfffffffffffffffen],
  [0xffffffffffffffffn, 0xffffffffffffffffn],
  [0x10000000000000000n, 0n],
  [0x10000000000000001n, 1n],
];

function f(tests) {
  for (let test of tests) {
    let input = test[0], expected = test[1];

    assertEq(BigInt.asUintN(64, input), expected);
  }
}

for (let i = 0; i < 100; ++i) {
  f(tests);
}
