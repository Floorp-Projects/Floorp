setJitCompilerOption("base-reg-for-locals", 1); // FP
function g(x) {
    with (this) {} // Don't inline.
    return x;
}
function f(x) {
    var sum = 0;
    for (var i = 0; i < x; i++) {
        sum += g(i);
    }
    return sum;
}
assertEq(f(2000), 1999000);
