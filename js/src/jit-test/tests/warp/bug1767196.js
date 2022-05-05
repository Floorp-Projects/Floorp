// |jit-test| --fast-warmup; --no-threads
function foo() {}
function main() {
  let value = 0.1;
  let result;
  foo();

  const neverTrue = false;
  for (let i = 0; i < 100; i++) {
    let f32 = Math.fround(i) && 0;
    result = neverTrue ? f32 : value;
  }
  return result;
}
assertEq(main(), 0.1);
