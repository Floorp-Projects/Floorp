// This test case originally failed only with --enable-more-deterministic

setJitCompilerOption("ion.forceinlineCaches", 1);
function f(x) {
    return Math.pow(Math.fround(Math.fround()), ~(x >>> 0));
}
assertEq(f(-1),1);
assertEq(f(-1),1);
assertEq(f(-1),1);
assertEq(f(-1),1);
