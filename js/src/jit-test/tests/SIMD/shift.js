load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function curry(f, arg) { return f.bind(null, arg); }

function binaryLsh(count, v) { count &= 31; return (v << count) | 0; }
function lsh(count) { return curry(binaryLsh, count); }

function binaryRsh(count, v) { count &= 31; return (v >> count) | 0; }
function rsh(count) { return curry(binaryRsh, count); }

function binaryUlsh(count, v) { count &= 31; return (v << count) >>> 0; }
function ulsh(count) { return curry(binaryUlsh, count); }

function binaryUrsh(count, v) { count &= 31; return v >>> count; }
function ursh(count) { return curry(binaryUrsh, count); }

function f() {
    var v = SIMD.Int32x4(1, 2, -3, 4);
    var u = SIMD.Uint32x4(1, 0x55005500, -3, 0xaa00aa00);
    var a = [1, 2, -3, 4];
    var b = [1, 0x55005500, -3, 0xaa00aa00];

    var shifts = [-2, -1, 0, 1, 31, 32, 33];

    var r;
    for (var i = 0; i < 150; i++) {
        // Constant shift counts
        assertEqX4(SIMD.Int32x4.shiftLeftByScalar(v, -1), a.map(lsh(-1)));
        assertEqX4(SIMD.Int32x4.shiftLeftByScalar(v, 0),  a.map(lsh(0)));
        assertEqX4(SIMD.Int32x4.shiftLeftByScalar(v, 1),  a.map(lsh(1)));
        assertEqX4(SIMD.Int32x4.shiftLeftByScalar(v, 2),  a.map(lsh(2)));
        assertEqX4(SIMD.Int32x4.shiftLeftByScalar(v, 31), a.map(lsh(31)));
        assertEqX4(SIMD.Int32x4.shiftLeftByScalar(v, 32), a.map(lsh(32)));
        assertEqX4(SIMD.Int32x4.shiftLeftByScalar(v, 33), a.map(lsh(33)));

        assertEqX4(SIMD.Int32x4.shiftRightByScalar(v, -1), a.map(rsh(31)));
        assertEqX4(SIMD.Int32x4.shiftRightByScalar(v, 0),  a.map(rsh(0)));
        assertEqX4(SIMD.Int32x4.shiftRightByScalar(v, 1),  a.map(rsh(1)));
        assertEqX4(SIMD.Int32x4.shiftRightByScalar(v, 2),  a.map(rsh(2)));
        assertEqX4(SIMD.Int32x4.shiftRightByScalar(v, 31), a.map(rsh(31)));
        assertEqX4(SIMD.Int32x4.shiftRightByScalar(v, 32), a.map(rsh(32)));
        assertEqX4(SIMD.Int32x4.shiftRightByScalar(v, 33), a.map(rsh(33)));

        assertEqX4(SIMD.Uint32x4.shiftLeftByScalar(u, -1), b.map(ulsh(-1)));
        assertEqX4(SIMD.Uint32x4.shiftLeftByScalar(u, 0),  b.map(ulsh(0)));
        assertEqX4(SIMD.Uint32x4.shiftLeftByScalar(u, 1),  b.map(ulsh(1)));
        assertEqX4(SIMD.Uint32x4.shiftLeftByScalar(u, 2),  b.map(ulsh(2)));
        assertEqX4(SIMD.Uint32x4.shiftLeftByScalar(u, 31), b.map(ulsh(31)));
        assertEqX4(SIMD.Uint32x4.shiftLeftByScalar(u, 32), b.map(ulsh(32)));
        assertEqX4(SIMD.Uint32x4.shiftLeftByScalar(u, 33), b.map(ulsh(33)));

        assertEqX4(SIMD.Uint32x4.shiftRightByScalar(u, -1), b.map(ursh(-1)));
        assertEqX4(SIMD.Uint32x4.shiftRightByScalar(u, 0),  b.map(ursh(0)));
        assertEqX4(SIMD.Uint32x4.shiftRightByScalar(u, 1),  b.map(ursh(1)));
        assertEqX4(SIMD.Uint32x4.shiftRightByScalar(u, 2),  b.map(ursh(2)));
        assertEqX4(SIMD.Uint32x4.shiftRightByScalar(u, 31), b.map(ursh(31)));
        assertEqX4(SIMD.Uint32x4.shiftRightByScalar(u, 32), b.map(ursh(32)));
        assertEqX4(SIMD.Uint32x4.shiftRightByScalar(u, 33), b.map(ursh(33)));

        // Non constant shift counts
        var c = shifts[i % shifts.length];

        assertEqX4(SIMD.Int32x4.shiftLeftByScalar(v, c), a.map(lsh(c)));
        assertEqX4(SIMD.Int32x4.shiftRightByScalar(v, c), a.map(rsh(c)));

        assertEqX4(SIMD.Uint32x4.shiftLeftByScalar(u, c), b.map(ulsh(c)));
        assertEqX4(SIMD.Uint32x4.shiftRightByScalar(u, c), b.map(ursh(c)));
    }
    return r;
}

f();

