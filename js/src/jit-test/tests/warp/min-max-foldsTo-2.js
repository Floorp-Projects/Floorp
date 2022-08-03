with ({}); // Don't inline anything into the top-level script.

// Fold min(x, min(y, z)) to min(min(x, y), z) with constant min(x, y).
for (let x of [-Infinity, -10, 0, 10, Infinity, NaN]) {
  for (let y of [-Infinity, -10, 0, 10, Infinity, NaN]) {
    for (let z of [-Infinity, -10, 0, 10, Infinity, NaN]) {
      let fn = Function("z", `return Math.min(${x}, Math.min(${y}, z))`);
      for (let i = 0; i < 100; ++i) {
        let r = fn(z);
        assertEq(r, Math.min(x, Math.min(y, z)));
        assertEq(r, Math.min(Math.min(x, y), z));
      }

      // Reverse operands.
      fn = Function("z", `return Math.min(${x}, Math.min(z, ${y}))`);
      for (let i = 0; i < 100; ++i) {
        let r = fn(z);
        assertEq(r, Math.min(x, Math.min(z, y)));
        assertEq(r, Math.min(Math.min(x, y), z));
      }

      // Reverse operands.
      fn = Function("z", `return Math.min(Math.min(${y}, z), ${x})`);
      for (let i = 0; i < 100; ++i) {
        let r = fn(z);
        assertEq(r, Math.min(Math.min(y, z), x));
        assertEq(r, Math.min(Math.min(x, y), z));
      }

      // Reverse operands.
      fn = Function("z", `return Math.min(Math.min(z, ${y}), ${x})`);
      for (let i = 0; i < 100; ++i) {
        let r = fn(z);
        assertEq(r, Math.min(Math.min(z,  y), x));
        assertEq(r, Math.min(Math.min(x, y), z));
      }
    }
  }
}

// Fold max(x, max(y, z)) to max(max(x, y), z) with constant max(x, y).
for (let x of [-Infinity, -10, 0, 10, Infinity, NaN]) {
  for (let y of [-Infinity, -10, 0, 10, Infinity, NaN]) {
    for (let z of [-Infinity, -10, 0, 10, Infinity, NaN]) {
      let fn = Function("z", `return Math.max(${x}, Math.max(${y}, z))`);
      for (let i = 0; i < 100; ++i) {
        let r = fn(z);
        assertEq(r, Math.max(x, Math.max(y, z)));
        assertEq(r, Math.max(Math.max(x, y), z));
      }

      // Reverse operands.
      fn = Function("z", `return Math.max(${x}, Math.max(z, ${y}))`);
      for (let i = 0; i < 100; ++i) {
        let r = fn(z);
        assertEq(r, Math.max(x, Math.max(z, y)));
        assertEq(r, Math.max(Math.max(x, y), z));
      }

      // Reverse operands.
      fn = Function("z", `return Math.max(Math.max(${y}, z), ${x})`);
      for (let i = 0; i < 100; ++i) {
        let r = fn(z);
        assertEq(r, Math.max(Math.max(y, z), x));
        assertEq(r, Math.max(Math.max(x, y), z));
      }

      // Reverse operands.
      fn = Function("z", `return Math.max(Math.max(z, ${y}), ${x})`);
      for (let i = 0; i < 100; ++i) {
        let r = fn(z);
        assertEq(r, Math.max(Math.max(z, y), x));
        assertEq(r, Math.max(Math.max(x, y), z));
      }
    }
  }
}

// Fold min(x, max(y, z)) to max(min(x, y), min(x, z)).
for (let x of [-Infinity, -10, 0, 10, Infinity, NaN]) {
  for (let y of [-Infinity, -10, 0, 10, Infinity, NaN]) {
    for (let z of ["", "asdf", [], [1,2,3], new Int32Array(0), new Int32Array(10)]) {
      let fn = Function("z", `return Math.min(${x}, Math.max(${y}, z.length))`);
      for (let i = 0; i < 100; ++i) {
        let r = fn(z);
        assertEq(r, Math.min(x, Math.max(y, z.length)));
        assertEq(r, Math.max(Math.min(x, y), Math.min(x, z.length)));
      }

      // Reverse operands.
      fn = Function("z", `return Math.min(${x}, Math.max(z.length, ${y}))`);
      for (let i = 0; i < 100; ++i) {
        let r = fn(z);
        assertEq(r, Math.min(x, Math.max(z.length, y)));
        assertEq(r, Math.max(Math.min(x, y), Math.min(x, z.length)));
      }

      // Reverse operands.
      fn = Function("z", `return Math.min(Math.max(${y}, z.length), ${x})`);
      for (let i = 0; i < 100; ++i) {
        let r = fn(z);
        assertEq(r, Math.min(Math.max(y, z.length), x));
        assertEq(r, Math.max(Math.min(x, y), Math.min(x, z.length)));
      }

      // Reverse operands.
      fn = Function("z", `return Math.min(Math.max(z.length, ${y}), ${x})`);
      for (let i = 0; i < 100; ++i) {
        let r = fn(z);
        assertEq(r, Math.min(Math.max(z.length, y), x));
        assertEq(r, Math.max(Math.min(x, y), Math.min(x, z.length)));
      }
    }
  }
}

// Fold max(x, min(y, z)) to min(max(x, y), max(x, z)).
for (let x of [-Infinity, -10, 0, 10, Infinity, NaN]) {
  for (let y of [-Infinity, -10, 0, 10, Infinity, NaN]) {
    for (let z of ["", "asdf", [], [1,2,3], new Int32Array(0), new Int32Array(10)]) {
      let fn = Function("z", `return Math.max(${x}, Math.min(${y}, z.length))`);
      for (let i = 0; i < 100; ++i) {
        let r = fn(z);
        assertEq(r, Math.max(x, Math.min(y, z.length)));
        assertEq(r, Math.min(Math.max(x, y), Math.max(x, z.length)));
      }

      // Reverse operands.
      fn = Function("z", `return Math.max(${x}, Math.min(z.length, ${y}))`);
      for (let i = 0; i < 100; ++i) {
        let r = fn(z);
        assertEq(r, Math.max(x, Math.min(z.length, y)));
        assertEq(r, Math.min(Math.max(x, y), Math.max(x, z.length)));
      }

      // Reverse operands.
      fn = Function("z", `return Math.max(Math.min(${y}, z.length), ${x})`);
      for (let i = 0; i < 100; ++i) {
        let r = fn(z);
        assertEq(r, Math.max(Math.min(y, z.length), x));
        assertEq(r, Math.min(Math.max(x, y), Math.max(x, z.length)));
      }

      // Reverse operands.
      fn = Function("z", `return Math.max(Math.min(z.length, ${y}), ${x})`);
      for (let i = 0; i < 100; ++i) {
        let r = fn(z);
        assertEq(r, Math.max(Math.min(z.length, y), x));
        assertEq(r, Math.min(Math.max(x, y), Math.max(x, z.length)));
      }
    }
  }
}
