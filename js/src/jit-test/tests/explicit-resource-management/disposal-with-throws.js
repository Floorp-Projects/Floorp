// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

let called1 = false, called2 = false;

try {
  using d1 = {
    [Symbol.dispose]() {
      called1 = true;
      throw 2;
    }
  }, d2 = {
    [Symbol.dispose]() {
      called2 = true;
      throw 1;
    }
  };
} catch {
}

assertEq(called1, true);
assertEq(called2, true);
