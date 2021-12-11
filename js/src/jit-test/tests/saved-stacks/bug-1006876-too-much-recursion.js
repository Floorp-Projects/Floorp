// |jit-test| exitstatus: 3

// This test case was found by the fuzzer and crashed the js shell. It should
// throw a "too much recursion" error, but was crashing instead.

enableTrackAllocations();
function f() {
    f();
}
f();
