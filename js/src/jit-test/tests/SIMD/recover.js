load(libdir + 'simd.js');

if (!this.hasOwnProperty("SIMD"))
  quit();

// This test case ensure that if we are able to optimize SIMD, then we can use
// recover instructions to get rid of the allocations. So, there is no value
// (and the test case would fail) if we are not able to inline SIMD
// constructors.
if (!isSimdAvailable())
  quit();

setJitCompilerOption("baseline.warmup.trigger", 10);
setJitCompilerOption("ion.warmup.trigger", 20);

// This function is used to cause an invalidation after having removed a branch
// after DCE.  This is made to check if we correctly recover an array
// allocation.
var uceFault = function (i) {
    if (i > 98)
        uceFault = function (i) { return true; };
    return false;
};

// Check that we can correctly recover a boxed value.
var uceFault_simdBox_i4 = eval(uneval(uceFault).replace('uceFault', 'uceFault_simdBox_i4'));
function simdBox_i4(i) {
    var a = SIMD.Int32x4(i, i, i, i);
    if (uceFault_simdBox_i4(i) || uceFault_simdBox_i4(i))
        assertEqX4(a, [i, i, i, i]);
    assertRecoveredOnBailout(a, true);
    return 0;
}

var uceFault_simdBox_f4 = eval(uneval(uceFault).replace('uceFault', 'uceFault_simdBox_f4'));
function simdBox_f4(i) {
    var a = SIMD.Float32x4(i, i + 0.1, i + 0.2, i + 0.3);
    if (uceFault_simdBox_f4(i) || uceFault_simdBox_f4(i))
        assertEqX4(a, [i, i + 0.1, i + 0.2, i + 0.3].map(Math.fround));
    assertRecoveredOnBailout(a, true);
    return 0;
}

for (var i = 0; i < 100; i++) {
    simdBox_i4(i);
    simdBox_f4(i);
}
