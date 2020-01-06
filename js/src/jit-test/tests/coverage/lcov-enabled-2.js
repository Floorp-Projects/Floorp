
// Setting the `JS_CODE_COVERAGE_OUTPUT_DIR` for will also enable coverage for
// the process.

if (os.getenv("JS_CODE_COVERAGE_OUTPUT_DIR")) {
    assertEq(isLcovEnabled(), true);
}
