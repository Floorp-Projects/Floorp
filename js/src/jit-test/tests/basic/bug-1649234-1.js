// |jit-test| exitstatus: 6; skip-if: getBuildConfiguration("wasi")

timeout(0.1, function() { return false; });
Atomics.add(new Int32Array(1), 0, {
  valueOf() {
    while (1);
  }
});
