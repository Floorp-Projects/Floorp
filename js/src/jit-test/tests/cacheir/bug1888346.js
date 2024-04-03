setJitCompilerOption("ion.frequent-bailout-threshold", 1);
for (let i = 0; i < 49; i++) {
  (function () {
    let x = new (function () {})();
    Object.defineProperty(x, "z", {});
    x.z;
  })();
}
