// |jit-test| --fast-warmup; --baseline-eager
function f() {
  let val1 = Math.sqrt(9007199254740992);
  let val2 = 0;
  let arr = new Float32Array(100);
  for (let i = 0; i < 100; i++) {
    val2 = arr[i];
  }
  return val1 + val2;
}
assertEq(f(), Math.sqrt(9007199254740992));
