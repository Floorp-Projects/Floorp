// |jit-test| --fast-warmup; --no-threads

function placeholder() {}

function main() {
  const fhash = placeholder;
  const v9 = -62980826;

  let v21;
  for (let i = 0; i < 32; i++) {
    // Call Math.fround() to enable the Float32 specialization pass.
    Math.fround(123);

    // Math.trunc() can produce Float32.
    const v18 = Math.trunc(-9007199254740992);

    // |valueOf| is always true, but can't be inferred at compile-time.
    const v20 = valueOf ? v18 : -9007199254740992;

    // Use the Float32 value.
    v21 = v20 && v9;
  }

  assertEq(v21, -62980826);
}

main();
