with ({}); // Don't inline anything into the top-level script.

function args() { return arguments; }

for (let xs of [
  // Array
  [[], [1, 2, 3]],

  // String
  ["", "asdf"],

  // ArrayBufferView
  [new Int32Array(0), new Int32Array(10)],

  // Arguments
  [args(), args(1, 2, 3)],
]) {
  for (let cst of [0, -1]) {
    // Fold `Math.min(length ≥ 0, constant ≤ 0)` to `constant`.
    let min = Function("x", `return Math.min(x.length, ${cst})`);
    for (let i = 0; i < 100; ++i) {
      let x = xs[i & 1];
      assertEq(min(x), cst);
    }

    // Reverse operands.
    min = Function("x", `return Math.min(${cst}, x.length)`);
    for (let i = 0; i < 100; ++i) {
      let x = xs[i & 1];
      assertEq(min(x), cst);
    }

    // Fold `Math.max(length ≥ 0, constant ≤ 0)` to `length`.
    let max = Function("x", `return Math.max(x.length, ${cst})`);
    for (let i = 0; i < 100; ++i) {
      let x = xs[i & 1];
      assertEq(max(x), x.length);
    }

    // Reverse operands.
    max = Function("x", `return Math.max(${cst}, x.length)`);
    for (let i = 0; i < 100; ++i) {
      let x = xs[i & 1];
      assertEq(max(x), x.length);
    }
  }
}
