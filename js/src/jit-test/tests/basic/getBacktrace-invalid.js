// |jit-test| skip-if: isLcovEnabled()
// Skip this test on Lcov because the invalid filename breaks the test harness.

// Some filename handling code still uses latin1, and some characters are
// replaced with REPLACEMENT CHARACTER (U+FFFD).
//
// FIXME: bug 1492090

const backtrace = evaluate(`
this.getBacktrace(this);
`, { fileName: "\u86D9" });

assertEq(backtrace.includes(`["\uFFFD":2:5]`), true);
