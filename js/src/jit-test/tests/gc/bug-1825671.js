let newTarget = Object.defineProperty(function () {}.bind(), "prototype", {
  get() {
    throw 0;
  }
});

try {
  Reflect.construct(FinalizationRegistry, [ 1 ], newTarget);
} catch (n) {
  assertEq(n === 0, false);
  assertEq(n instanceof TypeError, true);
}
