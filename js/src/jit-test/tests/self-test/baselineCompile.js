// |jit-test| test-also=--fuzzing-safe
// Check that the help text for baselineCompile() is accurate.

if (typeof inJit == "function" && typeof baselineCompile == "function") {
    if (!inJit()) {

        var res = baselineCompile();  // compile the current script

        assertEq(inJit(), false,
                 "We have compiled this script to baseline jitcode, but shouldn't " +
                 "be running it yet, according to the help text for baselineCompile() " +
                 "in TestingFunctions.cpp. If you fail this assertion, nice work, and " +
                 "please update the help text!");

        for (var i=0; i<1; i++) {}  // exact boilerplate suggested by the help text

        assertEq(typeof res != "string" ? inJit() : true, true,
                 "help text in TestingFunctions.cpp claims the above loop causes " +
                 "the interpreter to start running the new baseline jitcode");
    }
}
