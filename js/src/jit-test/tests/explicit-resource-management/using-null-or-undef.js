// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

function doesntThrowOnNullOrUndefinedDisposable() {
  using a = null;
  using b = undefined;
}
doesntThrowOnNullOrUndefinedDisposable();
