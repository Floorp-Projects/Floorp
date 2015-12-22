load(libdir + "simd.js");

setJitCompilerOption("ion.warmup.trigger", 50);

// Test constant folding into the Bool32x4 constructor.
// Verify that we get the truthiness right, c.f. the ECMA ToBoolean() function.
function f1() {
    var B = SIMD.Bool32x4;
    var S = SIMD.Bool32x4.splat;
    return [
        B(false, false, false, true),
        B(true),
        B(undefined, null, "", "x"),
        B({}, 0, 1, -0.0),
        B(NaN, -NaN, Symbol(), objectEmulatingUndefined()),

        S(false),
        S(true),
        S(undefined),
        S(null),

        S(""),
        S("x"),
        S(0),
        S(1),

        S({}),
        S(-0.0),
        S(NaN),
        S(Symbol()),

        S(objectEmulatingUndefined())
    ];
}

function f() {
    for (var i = 0; i < 100; i++) {
        var a = f1()
        assertEqX4(a[0], [false, false, false, true]);
        assertEqX4(a[1], [true,  false, false, false]);
        assertEqX4(a[2], [false, false, false, true]);
        assertEqX4(a[3], [true,  false, true,  false]);
        assertEqX4(a[4], [false, false, true,  false]);

        // Splats.
        assertEqX4(a[5], [false, false, false, false]);
        assertEqX4(a[6], [true, true, true, true]);
        assertEqX4(a[7], [false, false, false, false]);
        assertEqX4(a[8], [false, false, false, false]);

        assertEqX4(a[9], [false, false, false, false]);
        assertEqX4(a[10], [true, true, true, true]);
        assertEqX4(a[11], [false, false, false, false]);
        assertEqX4(a[12], [true, true, true, true]);

        assertEqX4(a[13], [true, true, true, true]);
        assertEqX4(a[14], [false, false, false, false]);
        assertEqX4(a[15], [false, false, false, false]);
        assertEqX4(a[16], [true, true, true, true]);

        assertEqX4(a[17], [false, false, false, false]);
    }
}

f();
