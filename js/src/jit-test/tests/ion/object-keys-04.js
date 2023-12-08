load(libdir + 'array-compare.js'); // arraysEqual

// Ion eager fails the test below because we have not yet created any
// Cache IR IC in baseline before running the content of the top-level
// function.
if (getJitCompilerOptions()["ion.warmup.trigger"] <= 100)
    setJitCompilerOption("ion.warmup.trigger", 100);

// This test case checks that we are capable of recovering the Object.keys array
// even if we optimized it away. It also checks that we indeed optimize it away
// as we would expect.

// Prevent GC from cancelling/discarding Ion compilations.
gczeal(0);

function objKeysLength(obj, expected, i) {
    var keys = Object.keys(obj);
    let len = keys.length;
    assertRecoveredOnBailout(keys, true);
    if (i >= 99) {
        // bailout would hint Ion to remove everything after, making the keys
        // array appear like being only used by resume points.
        bailout();
        assertEq(arraysEqual(keys, expected), true);
    }
    return len;
}

// This is the same test as above, except that the branch which is being removed
// cause the introduction of a different resume point to be inserted in the
// middle. At the moment we expect this circustances to to disable the
// optimization.
//
// Removing this limitation would be useful but would require more verification
// when applying the optimization.
function objKeysLengthDiffBlock(obj, expected, i) {
    var keys = Object.keys(obj);
    if (i >= 99) {
        // bailout would hint Ion to remove everything after, making the keys
        // array appear like being only used by resume points.
        bailout();
        assertEq(arraysEqual(keys, expected), true);
    }
    let len = keys.length;
    assertRecoveredOnBailout(keys, false);
    return len;
}

// Mutating the object in-between the call from Object.keys and the evaluation
// of the length property should prevent the optimization from being reflected
// as the mutation of the object would cause the a different result of
// Object.keys evaluation.
function objKeysLengthMutate0(obj, expected, i) {
    var keys = Object.keys(obj);
    obj.foo = 42;
    let len = keys.length;
    assertRecoveredOnBailout(keys, false);
    if (i >= 99) {
        bailout();
        assertEq(arraysEqual(keys, expected), true);
    }
    return len;
}

function objKeysLengthMutate1(obj, expected, i) {
    var keys = Object.keys(obj);
    let len = keys.length;
    assertRecoveredOnBailout(keys, false);
    obj.foo = 42;
    if (i >= 99) {
        bailout();
        assertEq(arraysEqual(keys, expected), true);
    }
    return len;
}

function objKeysLengthMutate2(obj, expected, i) {
    var keys = Object.keys(obj);
    let len = keys.length;
    assertRecoveredOnBailout(keys, false);
    if (i >= 99) {
        bailout();
        assertEq(arraysEqual(keys, expected), true);
    }
    obj.foo = 42;
    return len;
}

function objKeysLengthMutate3(obj, expected, i) {
    var keys = Object.keys(obj);
    let len = keys.length;
    assertRecoveredOnBailout(keys, true);
    if (i >= 99) {
        // When branches are pruned, Warp/Ion is not aware and would recover the
        // keys on bailout, and this is fine.
        obj.foo = 42;
        bailout();
        assertEq(arraysEqual(keys, expected), true);
    }
    return len;
}

function objKeysLengthMutate4(obj, expected, i) {
    // Mutating the objects ahead of keying the keys does not prevent optimizing
    // the keys length query, given that all side-effects are already acted by
    // the time we query the keys.
    obj.foo = 42;
    var keys = Object.keys(obj);
    let len = keys.length;
    assertRecoveredOnBailout(keys, true);
    if (i >= 99) {
        bailout();
        assertEq(arraysEqual(keys, expected), true);
    }
    return len;
}


function doNotInlineSideEffect() {
    eval("1");
}

function objKeysLengthSideEffect0(obj, expected, i) {
    var keys = Object.keys(obj);
    let len = keys.length;
    assertRecoveredOnBailout(keys, false);
    doNotInlineSideEffect();
    if (i >= 99) {
        bailout();
        assertEq(arraysEqual(keys, expected), true);
    }
    return len;
}

function objKeysLengthSideEffect1(obj, expected, i) {
    var keys = Object.keys(obj);
    let len = keys.length;
    assertRecoveredOnBailout(keys, false);
    if (i >= 99) {
        bailout();
        assertEq(arraysEqual(keys, expected), true);
    }
    doNotInlineSideEffect();
    return len;
}

function objKeysLengthSideEffect2(obj, expected, i) {
    var keys = Object.keys(obj);
    let len = keys.length;
    assertRecoveredOnBailout(keys, true);
    if (i >= 99) {
        // When branches are pruned, Warp/Ion is not aware and would recover the
        // keys on bailout, and this is fine.
        doNotInlineSideEffect();
        bailout();
        assertEq(arraysEqual(keys, expected), true);
    }
    return len;
}

function objKeysLengthSideEffect3(obj, expected, i) {
    doNotInlineSideEffect();
    var keys = Object.keys(obj);
    let len = keys.length;
    assertRecoveredOnBailout(keys, true);
    if (i >= 99) {
        bailout();
        assertEq(arraysEqual(keys, expected), true);
    }
    return len;
}

// Check what we are doing as optimizations when the object is fully known and
// when it does not escape.
//
// Except that today, Object.keys(..) is still assumed to make side-effect
// despite being removed later.
function nonEscapedObjKeysLength(i) {
    let obj = {a: i};
    var keys = Object.keys(obj);
    let len = keys.length;
    assertRecoveredOnBailout(keys, true);
    assertRecoveredOnBailout(obj, false);
    if (i >= 99) {
        bailout();
        assertEq(arraysEqual(keys, ["a"]), true);
    }
    return len;
}

// Prevent compilation of the top-level.
eval(`${arraysEqual}`);
let obj = {a: 0, b: 1, c: 2, d: 3};

for (let i = 0; i < 100; i++) {
    objKeysLength({...obj}, ["a", "b", "c", "d"], i);
    objKeysLengthDiffBlock({...obj}, ["a", "b", "c", "d"], i);
    objKeysLengthMutate0({...obj}, ["a", "b", "c", "d"], i);
    objKeysLengthMutate1({...obj}, ["a", "b", "c", "d"], i);
    objKeysLengthMutate2({...obj}, ["a", "b", "c", "d"], i);
    objKeysLengthMutate3({...obj}, ["a", "b", "c", "d"], i);
    objKeysLengthMutate4({...obj}, ["a", "b", "c", "d", "foo"], i);
    objKeysLengthSideEffect0({...obj}, ["a", "b", "c", "d"], i);
    objKeysLengthSideEffect1({...obj}, ["a", "b", "c", "d"], i);
    objKeysLengthSideEffect2({...obj}, ["a", "b", "c", "d"], i);
    objKeysLengthSideEffect3({...obj}, ["a", "b", "c", "d"], i);
    nonEscapedObjKeysLength(i);
}
