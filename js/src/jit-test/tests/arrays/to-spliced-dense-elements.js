// |jit-test| --enable-change-array-by-copy; skip-if: !getBuildConfiguration()['change-array-by-copy']

for (let i = 0; i < 1000; ++i) {
  let arr = [0];
  let result = arr.toSpliced(0, 0, 1, 2, 3);
  assertEq(result.length, 4);
}

for (let i = 0; i < 1000; ++i) {
  let arr = [0];
  let result = arr.toSpliced(1, 0, 1, 2, 3);
  assertEq(result.length, 4);
}
