const tests = [
  [-0x10000000000000001n, 0xffffffffn],
  [-0x10000000000000000n, 0n],
  [-0xffffffffffffffffn, 1n],
  [-0xfffffffffffffffen, 2n],
  [-0x8000000000000001n, 0xffffffffn],
  [-0x8000000000000000n, 0n],
  [-0x7fffffffffffffffn, 1n],
  [-0x7ffffffffffffffen, 2n],
  [-0x100000001n, 0xffffffffn],
  [-0x100000000n, 0n],
  [-0xffffffffn, 1n],
  [-0xfffffffen, 2n],
  [-0x80000001n, 0x7fffffffn],
  [-0x80000000n, 0x80000000n],
  [-0x7fffffffn, 0x80000001n],
  [-0x7ffffffen, 0x80000002n],
  [-9n, 0xfffffff7n],
  [-8n, 0xfffffff8n],
  [-7n, 0xfffffff9n],
  [-6n, 0xfffffffan],
  [-5n, 0xfffffffbn],
  [-4n, 0xfffffffcn],
  [-3n, 0xfffffffdn],
  [-2n, 0xfffffffen],
  [-1n, 0xffffffffn],
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
  [0x100000000n, 0n],
  [0x100000001n, 1n],
  [0x7ffffffffffffffen, 0xfffffffen],
  [0x7fffffffffffffffn, 0xffffffffn],
  [0x8000000000000000n, 0n],
  [0x8000000000000001n, 1n],
  [0xfffffffffffffffen, 0xfffffffen],
  [0xffffffffffffffffn, 0xffffffffn],
  [0x10000000000000000n, 0n],
  [0x10000000000000001n, 1n],
];

function f(tests) {
  for (let test of tests) {
    let input = test[0], expected = test[1];

    assertEq(BigInt.asUintN(32, input), expected);
  }
}

for (let i = 0; i < 100; ++i) {
  f(tests);
}
