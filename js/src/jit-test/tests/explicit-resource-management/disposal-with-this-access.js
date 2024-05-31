// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

const valuesDisposedWithThisAccess = [];
function testDisposalsHasThisAccess() {
  using a = {
    value: "a",
    [Symbol.dispose]() {
      valuesDisposedWithThisAccess.push(this.value);
    }
  };
}
testDisposalsHasThisAccess();
assertArrayEq(valuesDisposedWithThisAccess, ["a"]);
