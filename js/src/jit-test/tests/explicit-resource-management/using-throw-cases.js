// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

function throwsOnNonObjectDisposable() {
  using a = 1;
}
assertThrowsInstanceOf(throwsOnNonObjectDisposable, TypeError);

function throwsOnNonFunctionDispose() {
  using a = { [Symbol.dispose]: 1 };
}
assertThrowsInstanceOf(throwsOnNonFunctionDispose, TypeError);
