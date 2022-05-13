//|jit-test| --delazification-mode=concurrent-df; skip-if: helperThreadCount() === 0

function foo() {
    return "foo";
}

waitForStencilCache(foo);
// false would be expected if threads are disabled.
assertEq(isInStencilCache(foo), true);
