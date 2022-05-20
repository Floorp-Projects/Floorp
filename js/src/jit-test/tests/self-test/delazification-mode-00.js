//|jit-test| --delazification-mode=on-demand; skip-if: isLcovEnabled()

function foo() {
    return "foo";
}

// Wait is skipped as the source is not registered in the stencil cache.
waitForStencilCache(foo);
assertEq(isInStencilCache(foo), false);
