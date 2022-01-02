// |jit-test| skip-if: isLcovEnabled()
function f() {
  return 1;
}
assertEq(isLazyFunction(f), true);
assertEq(isRelazifiableFunction(f), false);
f();
assertEq(isLazyFunction(f), false);
assertEq(isRelazifiableFunction(f), true);
