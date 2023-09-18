// |jit-test| skip-if: getBuildConfiguration("pointer-byte-size") > 4 || getBuildConfiguration("android")

// On 64-bit, this will allocate 2G temporary strings for keys while
// stringifying the Array, which takes a rather long time and doesn't have the
// potential of the problematic overflowing anyway.

try {
    let x = [];
    x.length = Math.pow(2, 32) - 1;
    x + 1;
    assertEq(true, false, "overflow expected");
} catch (e) {
    assertEq((e + "").includes("InternalError: allocation size overflow"), true);
}
