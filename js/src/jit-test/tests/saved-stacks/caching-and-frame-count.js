// |jit-test| skip-if: getBuildConfiguration()['wasi']
//
// Test that the SavedFrame caching doesn't mess up counts. Specifically, that
// if we capture only the first n frames of a stack, we don't cache that stack
// and return it for when someone else captures another stack and asks for more
// frames than that last time.

function stackLength(stack) {
  return stack === null
    ? 0
    : 1 + stackLength(stack.parent);
}

(function f0() {
  (function f1() {
    (function f2() {
      (function f3() {
        (function f4() {
          (function f5() {
            (function f6() {
              (function f7() {
                (function f8() {
                  (function f9() {
                    const s1 = saveStack(3);
                    const s2 = saveStack(5);
                    const s3 = saveStack(0 /* no limit */);

                    assertEq(stackLength(s1), 3);
                    assertEq(stackLength(s2), 5);
                    assertEq(stackLength(s3), 11);
                  }());
                }());
              }());
            }());
          }());
        }());
      }());
    }());
  }());
}());
