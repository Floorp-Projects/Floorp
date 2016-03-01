var IsPossiblyWrappedTypedArray = getSelfHostedValue("IsPossiblyWrappedTypedArray");

var declareSamples = `
    var allTypedArraySamples = [
        { value: new Int8Array(1), expected: true },
        { value: new Uint8Array(1), expected: true },
        { value: new Int16Array(1), expected: true },
        { value: new Uint16Array(1), expected: true },
        { value: new Int32Array(1), expected: true },
        { value: new Uint32Array(1), expected: true },
        { value: new Float32Array(1), expected: true },
        { value: new Float64Array(1), expected: true },
        { value: new Uint8ClampedArray(1), expected: true }
    ];

    var allObjectSamples = [
        { value: new Array(1), expected: false },
        { value: {}, expected: false },
        { value: { length: 1 }, expected: false }
    ];

    var allNonObjectSamples = [
        { value: "a", expected: false },
        { value: 1.2, expected: false },
        { value: true, expected: false },
        { value: Symbol("a"), expected: false }
    ];
`;

// Create a new global to wrap with cross compartment wrappers.
var g = newGlobal();
evaluate(declareSamples)
g.evaluate(declareSamples);

var assertCode = `function (value, expected) {
    assertEq(IsPossiblyWrappedTypedArray(value), expected);
    return inIon();
}`;

function checkSamples(samples) {
    // Create the assert function anew every run so as not to share JIT code,
    // type information, etc.
    var assert = new Function(`return (${assertCode})`)();

    // Prevent Ion compilation of this function so that we don't freeze the
    // sample array's type.  If we did, IonBuilder's typed-array-length inlining
    // would always see a Mixed state, preventing IsPossiblyWrappedTypedArray
    // from being inlined.
    with ({}) {};

    do {
        // spinInJit is used to ensure that we at least test all elements in the
        // sample vector while running a compiled version of the assert
        // function.
        var spinInJit = true;
        for (var i = 0; i < samples.length; i++) {
            var e = samples[i];
            if (!e) continue;
            spinInJit = spinInJit && assert(e.value, e.expected);
        }
    } while(!spinInJit);
}

// Check a mix of samples from each type.
function test(a, b, c, d, e, f) {
    var samples = [
        a == -1 ? null : allTypedArraySamples[a],
        b == -1 ? null : allObjectSamples[b],
        c == -1 ? null : allNonObjectSamples[c],
        d == -1 ? null : g.allTypedArraySamples[d],
        e == -1 ? null : g.allObjectSamples[e],
        f == -1 ? null : g.allNonObjectSamples[f]
    ];

    checkSamples(samples);
}

// Check all samples.
checkSamples(allTypedArraySamples);
checkSamples(allObjectSamples);
checkSamples(allNonObjectSamples);
checkSamples(g.allTypedArraySamples);
checkSamples(g.allObjectSamples);
checkSamples(g.allNonObjectSamples);

// Check combinations mixing 2 elements from different types.
test(-1, -1, -1, -1,  0,  0);
test(-1, -1, -1,  0, -1,  0);
test(-1, -1, -1,  0,  0, -1);
test(-1, -1,  0, -1, -1,  0);
test(-1, -1,  0, -1,  0, -1);
test(-1, -1,  0,  0, -1, -1);
test(-1,  0, -1, -1, -1,  0);
test(-1,  0, -1, -1,  0, -1);
test(-1,  0, -1,  0, -1, -1);
test(-1,  0,  0, -1, -1, -1);
test( 0, -1, -1, -1, -1,  0);
test( 0, -1, -1, -1,  0, -1);
test( 0, -1, -1,  0, -1, -1);
test( 0, -1,  0, -1, -1, -1);
test( 0,  0, -1, -1, -1, -1);

// Check combinations mixing 3 elements from different types.
test(-1, -1, -1,  0,  0,  0);
test(-1, -1,  0, -1,  0,  0);
test(-1, -1,  0,  0, -1,  0);
test(-1, -1,  0,  0,  0, -1);
test(-1,  0, -1, -1,  0,  0);
test(-1,  0, -1,  0, -1,  0);
test(-1,  0, -1,  0,  0, -1);
test(-1,  0,  0, -1, -1,  0);
test(-1,  0,  0, -1,  0, -1);
test(-1,  0,  0,  0, -1, -1);
test( 0, -1, -1, -1,  0,  0);
test( 0, -1, -1,  0, -1,  0);
test( 0, -1, -1,  0,  0, -1);
test( 0, -1,  0, -1, -1,  0);
test( 0, -1,  0, -1,  0, -1);
test( 0, -1,  0,  0, -1, -1);
test( 0,  0, -1, -1, -1,  0);
test( 0,  0, -1, -1,  0, -1);
test( 0,  0, -1,  0, -1, -1);
test( 0,  0,  0, -1, -1, -1);

// Check combinations mixing 4 elements from different types.
test(-1, -1,  0,  0,  0,  0);
test(-1,  0, -1,  0,  0,  0);
test(-1,  0,  0, -1,  0,  0);
test(-1,  0,  0,  0, -1,  0);
test(-1,  0,  0,  0,  0, -1);
test( 0, -1, -1,  0,  0,  0);
test( 0, -1,  0, -1,  0,  0);
test( 0, -1,  0,  0, -1,  0);
test( 0, -1,  0,  0,  0, -1);
test( 0,  0, -1, -1,  0,  0);
test( 0,  0, -1,  0, -1,  0);
test( 0,  0, -1,  0,  0, -1);
test( 0,  0,  0, -1, -1,  0);
test( 0,  0,  0, -1,  0, -1);
test( 0,  0,  0,  0, -1, -1);

// Check combinations mixing 5 elements from different types.
test(-1,  0,  0,  0,  0,  0);
test( 0, -1,  0,  0,  0,  0);
test( 0,  0, -1,  0,  0,  0);
test( 0,  0,  0, -1,  0,  0);
test( 0,  0,  0,  0, -1,  0);
test( 0,  0,  0,  0,  0, -1);

// Check combinations mixing 6 elements from different types.
test( 0,  0,  0,  0,  0,  0);
