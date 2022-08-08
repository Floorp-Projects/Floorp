with ({}); // Don't inline anything into the top-level script.

const numbers = [
  -Infinity, -10, -5, -2, -1, -0.5, 0, 0.5, 1, 2, 5, 10, Infinity, NaN,
];

// Test all supported types (Int32, Float32, Float64).
const converters = [
  x => `${x}`,
  x => `(${x}|0)`,
  x => `Math.fround(${x})`,
  x => `numberToDouble(${x})`,
];

// Fold min(x, min(x, y)) to min(x, y).
for (let cvt of converters) {
  let x = cvt("x");
  let y = cvt("y");
  let c = Function("a", `return ${cvt("a")}`);

  let min1 = Function("x, y", `return Math.min(${x}, Math.min(${x}, ${y}))`);
  let min2 = Function("x, y", `return Math.min(${y}, Math.min(${x}, ${y}))`);
  let min3 = Function("x, y", `return Math.min(Math.min(${x}, ${y}), ${x})`);
  let min4 = Function("x, y", `return Math.min(Math.min(${x}, ${y}), ${y})`);

  for (let i = 0; i < 20; ++i) {
    for (let j = 0; j < numbers.length; ++j) {
      for (let k = 0; k < numbers.length; ++k) {
        let x = numbers[j];
        let y = numbers[k]
        let r1 = min1(x, y);
        let r2 = min2(x, y);
        let r3 = min3(x, y);
        let r4 = min4(x, y);

        // Convert to the correct type before computing the expected results.
        x = c(x);
        y = c(y);

        assertEq(r1, Math.min(x, Math.min(x, y)));
        assertEq(r1, Math.min(x, y));

        assertEq(r2, Math.min(y, Math.min(x, y)));
        assertEq(r2, Math.min(x, y));

        assertEq(r3, Math.min(Math.min(x, y), x));
        assertEq(r3, Math.min(x, y));

        assertEq(r4, Math.min(Math.min(x, y), y));
        assertEq(r4, Math.min(x, y));
      }
    }
  }
}

// Fold max(x, max(x, y)) to max(x, y).
for (let cvt of converters) {
  let x = cvt("x");
  let y = cvt("y");
  let c = Function("a", `return ${cvt("a")}`);

  let max1 = Function("x, y", `return Math.max(${x}, Math.max(${x}, ${y}))`);
  let max2 = Function("x, y", `return Math.max(${y}, Math.max(${x}, ${y}))`);
  let max3 = Function("x, y", `return Math.max(Math.max(${x}, ${y}), ${x})`);
  let max4 = Function("x, y", `return Math.max(Math.max(${x}, ${y}), ${y})`);

  for (let i = 0; i < 20; ++i) {
    for (let j = 0; j < numbers.length; ++j) {
      for (let k = 0; k < numbers.length; ++k) {
        let x = numbers[j];
        let y = numbers[k]
        let r1 = max1(x, y);
        let r2 = max2(x, y);
        let r3 = max3(x, y);
        let r4 = max4(x, y);

        // Convert to the correct type before computing the expected results.
        x = c(x);
        y = c(y);

        assertEq(r1, Math.max(x, Math.max(x, y)));
        assertEq(r1, Math.max(x, y));

        assertEq(r2, Math.max(y, Math.max(x, y)));
        assertEq(r2, Math.max(x, y));

        assertEq(r3, Math.max(Math.max(x, y), x));
        assertEq(r3, Math.max(x, y));

        assertEq(r4, Math.max(Math.max(x, y), y));
        assertEq(r4, Math.max(x, y));
      }
    }
  }
}

// Fold max(x, min(x, y)) = x.
for (let cvt of converters) {
  let x = cvt("x");
  let y = cvt("y");
  let c = Function("a", `return ${cvt("a")}`);

  let maxmin1 = Function("x, y", `return Math.max(${x}, Math.min(${x}, ${y}))`);
  let maxmin2 = Function("x, y", `return Math.max(${y}, Math.min(${x}, ${y}))`);
  let maxmin3 = Function("x, y", `return Math.max(Math.min(${x}, ${y}), ${x})`);
  let maxmin4 = Function("x, y", `return Math.max(Math.min(${x}, ${y}), ${y})`);

  for (let i = 0; i < 20; ++i) {
    for (let j = 0; j < numbers.length; ++j) {
      for (let k = 0; k < numbers.length; ++k) {
        let x = numbers[j];
        let y = numbers[k]
        let r1 = maxmin1(x, y);
        let r2 = maxmin2(x, y);
        let r3 = maxmin3(x, y);
        let r4 = maxmin4(x, y);

        // Convert to the correct type before computing the expected results.
        x = c(x);
        y = c(y);

        assertEq(r1, Math.max(x, Math.min(x, y)));
        assertEq(r1, Number.isNaN(y) ? NaN : x);

        assertEq(r2, Math.max(y, Math.min(x, y)));
        assertEq(r2, Number.isNaN(x) ? NaN : y);

        assertEq(r3, Math.max(Math.min(x, y), x));
        assertEq(r3, Number.isNaN(y) ? NaN : x);

        assertEq(r4, Math.max(Math.min(x, y), y));
        assertEq(r4, Number.isNaN(x) ? NaN : y);
      }
    }
  }
}

// Fold min(x, max(x, y)) = x.
for (let cvt of converters) {
  let x = cvt("x");
  let y = cvt("y");
  let c = Function("a", `return ${cvt("a")}`);

  let minmax1 = Function("x, y", `return Math.min(${x}, Math.max(${x}, ${y}))`);
  let minmax2 = Function("x, y", `return Math.min(${y}, Math.max(${x}, ${y}))`);
  let minmax3 = Function("x, y", `return Math.min(Math.max(${x}, ${y}), ${x})`);
  let minmax4 = Function("x, y", `return Math.min(Math.max(${x}, ${y}), ${y})`);

  for (let i = 0; i < 20; ++i) {
    for (let j = 0; j < numbers.length; ++j) {
      for (let k = 0; k < numbers.length; ++k) {
        let x = numbers[j];
        let y = numbers[k]
        let r1 = minmax1(x, y);
        let r2 = minmax2(x, y);
        let r3 = minmax3(x, y);
        let r4 = minmax4(x, y);

        // Convert to the correct type before computing the expected results.
        x = c(x);
        y = c(y);

        assertEq(r1, Math.min(x, Math.max(x, y)));
        assertEq(r1, Number.isNaN(y) ? NaN : x);

        assertEq(r2, Math.min(y, Math.max(x, y)));
        assertEq(r2, Number.isNaN(x) ? NaN : y);

        assertEq(r3, Math.min(Math.max(x, y), x));
        assertEq(r3, Number.isNaN(y) ? NaN : x);

        assertEq(r4, Math.min(Math.max(x, y), y));
        assertEq(r4, Number.isNaN(x) ? NaN : y);
      }
    }
  }
}
