load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function curry(f, arg) { return f.bind(null, arg); }

function binaryLsh(count, v) { if (count>>>0 >= 32) return 0; return (v << count) | 0; }
function lsh(count) { return curry(binaryLsh, count); }

function binaryRsh(count, v) { if (count>>>0 >= 32) count = 31; return (v >> count) | 0; }
function rsh(count) { return curry(binaryRsh, count); }

function binaryUrsh(count, v) { if (count>>>0 >= 32) return 0; return (v >>> count) | 0; }
function ursh(count) { return curry(binaryUrsh, count); }

function f() {
    var v = SIMD.Int32x4(1, 2, -3, 4);
    var a = [1, 2, -3, 4];
    var zeros = [0,0,0,0];

    var shifts = [-1, 0, 1, 31, 32];

    var r;
    for (var i = 0; i < 150; i++) {
        // Constant shift counts
        assertEqX4(SIMD.Int32x4.shiftLeftByScalar(v, -1), a.map(lsh(-1)));
        assertEqX4(SIMD.Int32x4.shiftLeftByScalar(v, 0),  a.map(lsh(0)));
        assertEqX4(SIMD.Int32x4.shiftLeftByScalar(v, 1),  a.map(lsh(1)));
        assertEqX4(SIMD.Int32x4.shiftLeftByScalar(v, 2),  a.map(lsh(2)));
        assertEqX4(SIMD.Int32x4.shiftLeftByScalar(v, 31), a.map(lsh(31)));
        assertEqX4(SIMD.Int32x4.shiftLeftByScalar(v, 32), a.map(lsh(32)));

        assertEqX4(SIMD.Int32x4.shiftRightArithmeticByScalar(v, -1), a.map(rsh(31)));
        assertEqX4(SIMD.Int32x4.shiftRightArithmeticByScalar(v, 0),  a.map(rsh(0)));
        assertEqX4(SIMD.Int32x4.shiftRightArithmeticByScalar(v, 1),  a.map(rsh(1)));
        assertEqX4(SIMD.Int32x4.shiftRightArithmeticByScalar(v, 2),  a.map(rsh(2)));
        assertEqX4(SIMD.Int32x4.shiftRightArithmeticByScalar(v, 31), a.map(rsh(31)));
        assertEqX4(SIMD.Int32x4.shiftRightArithmeticByScalar(v, 32), a.map(rsh(31)));

        assertEqX4(SIMD.Int32x4.shiftRightLogicalByScalar(v, -1), a.map(ursh(-1)));
        assertEqX4(SIMD.Int32x4.shiftRightLogicalByScalar(v, 0),  a.map(ursh(0)));
        assertEqX4(SIMD.Int32x4.shiftRightLogicalByScalar(v, 1),  a.map(ursh(1)));
        assertEqX4(SIMD.Int32x4.shiftRightLogicalByScalar(v, 2),  a.map(ursh(2)));
        assertEqX4(SIMD.Int32x4.shiftRightLogicalByScalar(v, 31), a.map(ursh(31)));
        assertEqX4(SIMD.Int32x4.shiftRightLogicalByScalar(v, 32), a.map(ursh(32)));

        // Non constant shift counts
        var c = shifts[i % shifts.length];
        assertEqX4(SIMD.Int32x4.shiftLeftByScalar(v, c), a.map(lsh(c)));
        assertEqX4(SIMD.Int32x4.shiftRightArithmeticByScalar(v, c), a.map(rsh(c)));
        assertEqX4(SIMD.Int32x4.shiftRightLogicalByScalar(v, c), a.map(ursh(c)));
    }
    return r;
}

f();

