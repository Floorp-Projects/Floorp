// |jit-test| skip-if: !getBuildConfiguration("debug")

// Due to sign extension of characters, we might accidentally extend \xAF into
// \uFFAF, which is incorrect.
assertEq(disassemble("'\xAF'").includes("FFAF"), false);
