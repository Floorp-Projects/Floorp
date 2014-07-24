// |jit-test| exitstatus: 3

enableTrackAllocations();
function f() {
    eval('f();');
}
f();
