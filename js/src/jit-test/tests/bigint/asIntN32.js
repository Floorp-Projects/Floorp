const tests = [
  [-0x10000000000000001n, -1n],
  [-0x10000000000000000n, 0n],
  [-0xffffffffffffffffn, 1n],
  [-0xfffffffffffffffen, 2n],
  [-0x8000000000000001n, -1n],
  [-0x8000000000000000n, 0n],
  [-0x7fffffffffffffffn, 1n],
  [-0x7ffffffffffffffen, 2n],
  [-0x100000001n, -1n],
  [-0x100000000n, 0n],
  [-0xffffffffn, 1n],
  [-0xfffffffen, 2n],
  [-0x80000001n, 0x7fffffffn],
  [-0x80000000n, -0x80000000n],
  [-0x7fffffffn, -0x7fffffffn],
  [-0x7ffffffen, -0x7ffffffen],
  [-9n, -9n],
  [-8n, -8n],
  [-7n, -7n],
  [-6n, -6n],
  [-5n, -5n],
  [-4n, -4n],
  [-3n, -3n],
  [-2n, -2n],
  [-1n, -1n],
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
  [0x80000000n, -0x80000000n],
  [0x80000001n, -0x7fffffffn],
  [0xfffffffen, -2n],
  [0xffffffffn, -1n],
  [0x100000000n, 0n],
  [0x100000001n, 1n],
  [0x7ffffffffffffffen, -2n],
  [0x7fffffffffffffffn, -1n],
  [0x8000000000000000n, 0n],
  [0x8000000000000001n, 1n],
  [0xfffffffffffffffen, -2n],
  [0xffffffffffffffffn, -1n],
  [0x10000000000000000n, 0n],
  [0x10000000000000001n, 1n],
];

function f(tests) {
  for (let test of tests) {
    let input = test[0], expected = test[1];

    assertEq(BigInt.asIntN(32, input), expected);
  }
}

for (let i = 0; i < 100; ++i) {
  f(tests);
}
