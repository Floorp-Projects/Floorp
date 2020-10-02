// |jit-test| --fuzzing-safe; --no-threads; --no-baseline; --no-ion; skip-if: !wasmIsSupported()

(function (stdlib) {
  "use asm";
  var sqrt = stdlib.Math.sqrt;
  function f(i0) {
    i0 = i0 | 0;
    i0 = ~~sqrt(-.5);
    return (1 / (1 >> (.0 == .0)) & i0 >> 1);
  }
  return f;
})(this)();
