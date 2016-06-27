// |jit-test| test-join=--no-unboxed-objects; --ion-pgo=on
//
// Unboxed object optimization might not trigger in all cases, thus we ensure
// that Scalar Replacement optimization is working well independently of the
// object representation.

// Ion eager fails the test below because we have not yet created any
// template object in baseline before running the content of the top-level
// function.
if (getJitCompilerOptions()["ion.warmup.trigger"] <= 130)
    setJitCompilerOption("ion.warmup.trigger", 130);

// This test checks that we are able to remove the getelem & setelem with scalar
// replacement, so we should not force inline caches, as this would skip the
// generation of getelem & setelem instructions.
if (getJitCompilerOptions()["ion.forceinlineCaches"])
    setJitCompilerOption("ion.forceinlineCaches", 0);

var uceFault = function (j) {
    if (j >= max)
        uceFault = function (j) { return true; };
    return false;
}

function f(j) {
    var i = Math.pow(2, j) | 0;
    var obj = {
      i: i,
      v: i + i
    };
    // These can only be recovered on bailout iff either we have type
    // information for the property access in the branch, or the branch is
    // removed before scalar replacement.
    assertRecoveredOnBailout(obj, true);
    assertRecoveredOnBailout(obj.v, true);
    if (uceFault(j) || uceFault(j)) {
        // MObjectState::recover should neither fail,
        // nor coerce its result to an int32.
        assertEq(obj.v, 2 * i);
    }
    return 2 * obj.i;
}

var max = 150;
for (var j = 0; j <= max; ++j) {
    with({}){};
    f(j);
}
