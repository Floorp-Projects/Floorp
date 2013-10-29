setJitCompilerOption("ion.usecount.trigger", 50);

var f32 = new Float32Array(10);

function test(setup, f) {
    if (f === undefined) {
        f = setup;
        setup = function(){};
    }
    setup();
    for(var n = 200; n; --n) {
        f();
    }
}

// Basic arithmetic
function setupBasicArith() {
    f32[0] = -Infinity;
    f32[1] = -1;
    f32[2] = -0;
    f32[3] = 0;
    f32[4] = 1.337;
    f32[5] = 42;
    f32[6] = Infinity;
    f32[7] = NaN;
}
function basicArith() {
    for (var i = 0; i < 7; ++i) {
        var opf = Math.fround(f32[i] + f32[i+1]);
        var opd = (1 / (1 / f32[i])) + f32[i+1];
        assertFloat32(opf, true);
        assertFloat32(opd, false);
        assertEq(opf, Math.fround(opd));

        opf = Math.fround(f32[i] - f32[i+1]);
        opd = (1 / (1 / f32[i])) - f32[i+1];
        assertFloat32(opf, true);
        assertFloat32(opd, false);
        assertEq(opf, Math.fround(opd));

        opf = Math.fround(f32[i] * f32[i+1]);
        opd = (1 / (1 / f32[i])) * f32[i+1];
        assertFloat32(opf, true);
        assertFloat32(opd, false);
        assertEq(opf, Math.fround(opd));

        opf = Math.fround(f32[i] / f32[i+1]);
        opd = (1 / (1 / f32[i])) / f32[i+1];
        assertFloat32(opf, true);
        assertFloat32(opd, false);
        assertEq(opf, Math.fround(opd));
    }
}
test(setupBasicArith, basicArith);

// MAbs
function setupAbs() {
    f32[0] = -0;
    f32[1] = 0;
    f32[2] = -3.14159;
    f32[3] = 3.14159;
    f32[4] = -Infinity;
    f32[5] = Infinity;
    f32[6] = NaN;
}
function abs() {
    for(var i = 0; i < 7; ++i) {
        assertEq( Math.fround(Math.abs(f32[i])), Math.abs(f32[i]) );
    }
}
test(setupAbs, abs);

// MSqrt
function setupSqrt() {
    f32[0] = 0;
    f32[1] = 1;
    f32[2] = 4;
    f32[3] = -1;
    f32[4] = Infinity;
    f32[5] = NaN;
    f32[6] = 13.37;
}
function sqrt() {
    for(var i = 0; i < 7; ++i) {
        var sqrtf = Math.fround(Math.sqrt(f32[i]));
        var sqrtd = 1 + Math.sqrt(f32[i]) - 1; // force no float32 by chaining arith ops
        assertEq( sqrtf, Math.fround(sqrtd) );
    }
}
test(setupSqrt, sqrt);

// MTruncateToInt32
// The only way to get a MTruncateToInt32 with a Float32 input is to use Math.imul
function setupTruncateToInt32() {
    f32[0] = -1;
    f32[1] = 4;
    f32[2] = 5.13;
}
function truncateToInt32() {
    assertEq( Math.imul(f32[0], f32[1]), Math.imul(-1, 4) );
    assertEq( Math.imul(f32[1], f32[2]), Math.imul(4, 5) );
}
test(setupTruncateToInt32, truncateToInt32);

// MCompare
function comp() {
    for(var i = 0; i < 9; ++i) {
        assertEq( f32[i] < f32[i+1], true );
    }
}
function setupComp() {
    f32[0] = -Infinity;
    f32[1] = -1;
    f32[2] = -0.01;
    f32[3] = 0;
    f32[4] = 0.01;
    f32[5] = 1;
    f32[6] = 10;
    f32[7] = 13.37;
    f32[8] = 42;
    f32[9] = Infinity;
}
test(setupComp, comp);

// MNot
function setupNot() {
    f32[0] = -0;
    f32[1] = 0;
    f32[2] = 1;
    f32[3] = NaN;
    f32[4] = Infinity;
    f32[5] = 42;
    f32[5] = -23;
}
function not() {
    assertEq( !f32[0], true );
    assertEq( !f32[1], true );
    assertEq( !f32[2], false );
    assertEq( !f32[3], true );
    assertEq( !f32[4], false );
    assertEq( !f32[5], false );
    assertEq( !f32[6], false );
}
test(setupNot, not);

// MToInt32
var str = "can haz cheezburger? okthxbye;";
function setupToInt32() {
    f32[0] = 0;
    f32[1] = 1;
    f32[2] = 2;
    f32[3] = 4;
    f32[4] = 5;
}
function testToInt32() {
    assertEq(str[f32[0]], 'c');
    assertEq(str[f32[1]], 'a');
    assertEq(str[f32[2]], 'n');
    assertEq(str[f32[3]], 'h');
    assertEq(str[f32[4]], 'a');
}
test(setupToInt32, testToInt32);

function setupBailoutToInt32() {
    f32[0] = .5;
}
function testBailoutToInt32() {
    assertEq(typeof str[f32[0]], 'undefined');
}
test(setupBailoutToInt32, testBailoutToInt32);

// MMath
function assertNear(a, b) {
    return ( a != a && b != b ) ||
           Math.abs(a-b) < 1e-7;
}
function setupTrigo() {
    f32[0] = 0;
    f32[1] = Math.PI / 2;
    f32[2] = Math.PI;
    f32[3] = 3 * Math.PI / 2;
}
function trigo() {
    // Cos
    assertNear(Math.fround(Math.cos(f32[0])), Math.cos(0));
    assertNear(Math.fround(Math.cos(f32[1])), Math.cos(Math.PI / 2));
    assertNear(Math.fround(Math.cos(f32[2])), Math.cos(Math.PI));
    assertNear(Math.fround(Math.cos(f32[3])), Math.cos(3 * Math.PI / 2));

    // Sin
    assertNear(Math.fround(Math.sin(f32[0])), Math.sin(0));
    assertNear(Math.fround(Math.sin(f32[1])), Math.sin(Math.PI / 2));
    assertNear(Math.fround(Math.sin(f32[2])), Math.sin(Math.PI));
    assertNear(Math.fround(Math.sin(f32[3])), Math.sin(3 * Math.PI / 2));

    // Tan
    assertNear(Math.fround(Math.tan(f32[0])), Math.tan(0));
    assertNear(Math.fround(Math.tan(f32[1])), Math.tan(Math.PI / 2));
    assertNear(Math.fround(Math.tan(f32[2])), Math.tan(Math.PI));
    assertNear(Math.fround(Math.tan(f32[3])), Math.tan(3 * Math.PI / 2));

    // ACos
    assertNear(Math.fround(Math.acos(f32[0])), Math.acos(0));
    assertNear(Math.fround(Math.acos(f32[1])), Math.acos(Math.PI / 2));
    assertNear(Math.fround(Math.acos(f32[2])), Math.acos(Math.PI));
    assertNear(Math.fround(Math.acos(f32[3])), Math.acos(3 * Math.PI / 2));

    // ASin
    assertNear(Math.fround(Math.asin(f32[0])), Math.asin(0));
    assertNear(Math.fround(Math.asin(f32[1])), Math.asin(Math.PI / 2));
    assertNear(Math.fround(Math.asin(f32[2])), Math.asin(Math.PI));
    assertNear(Math.fround(Math.asin(f32[3])), Math.asin(3 * Math.PI / 2));

    // ATan
    assertNear(Math.fround(Math.atan(f32[0])), Math.atan(0));
    assertNear(Math.fround(Math.atan(f32[1])), Math.atan(Math.PI / 2));
    assertNear(Math.fround(Math.atan(f32[2])), Math.atan(Math.PI));
    assertNear(Math.fround(Math.atan(f32[3])), Math.atan(3 * Math.PI / 2));
}
test(setupTrigo, trigo);

function otherMath() {
    for (var i = 0; i < 10; ++i) {
        assertNear(Math.fround(Math.exp(f32[i])), Math.exp(f32[i]));
        assertNear(Math.fround(Math.log(f32[i])), Math.log(f32[i]));
    }
};
test(setupComp, otherMath);

function setupFloor() {
    f32[0] = -5.5;
    f32[1] = -0.5;
    f32[2] = 0;
    f32[3] = 1.5;
}
function setupFloorDouble() {
    f32[4] = NaN;
    f32[5] = -0;
    f32[6] = Infinity;
    f32[7] = -Infinity;
    f32[8] = Math.pow(2,31); // too big to fit into a int
}
function testFloor() {
    for (var i = 0; i < 4; ++i) {
        var f = Math.floor(f32[i]);
        assertFloat32(g, false); // f is an int32

        var g = Math.floor(-0 + f32[i]);
        assertFloat32(g, false);

        assertEq(f, g);
    }
}
function testFloorDouble() {
    for (var i = 4; i < 9; ++i) {
        var f = Math.fround(Math.floor(f32[i]));
        assertFloat32(f, true);

        var g = Math.floor(-0 + f32[i]);
        assertFloat32(g, false);

        assertEq(f, g);
    }
}
test(setupFloor, testFloor);
test(setupFloorDouble, testFloorDouble);

