// This test case originally failed only with --enable-more-deterministic

setJitCompilerOption("ion.forceinlineCaches", 1);
function f(x) {
    print(Math.pow(Math.fround(Math.fround()), ~(x >>> 0)));
}
assertEq(f(-1),true);
assertEq(f(-1),true);
assertEq(f(-1),true);
assertEq(f(-1),true);
