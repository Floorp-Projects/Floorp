const ta = new Int32Array(1);
const desc = {value: 0, writable: true, enumerable: true, configurable: true};

// Out-of-bounds with an int32 index.
for (let i = 0; i < 1000; ++i) {
  let didThrow = false;
  try {
    Object.defineProperty(ta, 10, desc);
  } catch {
    didThrow = true;
  }
  assertEq(didThrow, true);
}

// Out-of-bounds with a non-int32 index.
for (let i = 0; i < 1000; ++i) {
  let didThrow = false;
  try {
    Object.defineProperty(ta, 12.3, desc);
  } catch {
    didThrow = true;
  }
  assertEq(didThrow, true);
}
