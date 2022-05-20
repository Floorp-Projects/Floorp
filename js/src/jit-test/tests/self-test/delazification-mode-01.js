//|jit-test| --delazification-mode=concurrent-df; skip-if: isLcovEnabled() || helperThreadCount() === 0 || ('gczeal' in this && (gczeal(0), false))
//
// Note, we have to execute gczeal in the skip-if condition, before the current
// script gets parsed and scheduled for delazification, as changing gc settings
// while the helper thread is making allocation could cause intermittents.

function foo() {
    return "foo";
}

waitForStencilCache(foo);
// false would be expected if threads are disabled.
assertEq(isInStencilCache(foo), true);
