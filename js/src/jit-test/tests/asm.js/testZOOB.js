// |jit-test| test-also-noasmjs
load(libdir + "asm.js");
load(libdir + "asserts.js");

setIonCheckGraphCoherency(false);
setCachingEnabled(false);

var ab = new ArrayBuffer(BUF_MIN);

// Compute a set of interesting indices.
indices = [0]
for (var i of [4,1024,BUF_MIN,Math.pow(2,30),Math.pow(2,31),Math.pow(2,32),Math.pow(2,33)]) {
    for (var j of [-2,-1,0,1,2]) {
        for (var k of [1,-1])
            indices.push((i+j)*k);
    }
}

function testInt(ctor, shift, scale, disp) {
    var arr = new ctor(ab);

    var c = asmCompile('glob', 'imp', 'b',
                       USE_ASM +
                       'var arr=new glob.' + ctor.name + '(b); ' +
                       'function load(i) {i=i|0; return arr[((i<<' + scale + ')+' + disp + ')>>' + shift + ']|0 } ' +
                       'function store(i,j) {i=i|0;j=j|0; arr[((i<<' + scale + ')+' + disp + ')>>' + shift + '] = j } ' +
                       'function storeZero(i) {i=i|0; arr[((i<<' + scale + ')+' + disp + ')>>' + shift + '] = 0 } ' +
                       'function storeNegOne(i) {i=i|0; arr[((i<<' + scale + ')+' + disp + ')>>' + shift + '] = -1 } ' +
                       'return { load: load, store: store, storeZero: storeZero, storeNegOne: storeNegOne }');
    var f = asmLink(c, this, null, ab);

    var v = arr[0];
    arr[0] = -1;
    var negOne = arr[0]|0;
    arr[0] = v;

    for (var i of indices) {
        var index = ((i<<scale)+disp)>>shift;
        v = arr[index]|0;

        // Loads
        assertEq(f.load(i), v);

        // Stores of immediates
        arr[index] = 1;
        f.storeZero(i);
        assertEq(arr[index]|0, 0);
        f.storeNegOne(i);
        assertEq(arr[index]|0, index>>>0 < arr.length ? negOne : 0);

        // Stores
        arr[index] = ~v;
        f.store(i, v);
        assertEq(arr[index]|0, v);
    }
}

function testFloat(ctor, shift, scale, disp, coercion) {
    var arr = new ctor(ab);

    var c = asmCompile('glob', 'imp', 'b',
                       USE_ASM +
                       'var arr=new glob.' + ctor.name + '(b); ' +
                       'var toF = glob.Math.fround; ' +
                       'function load(i) {i=i|0; return ' + coercion + '(arr[((i<<' + scale + ')+' + disp + ')>>' + shift + ']) } ' +
                       'function store(i,j) {i=i|0;j=+j; arr[((i<<' + scale + ')+' + disp + ')>>' + shift + '] = j } ' +
                       'return { load: load, store: store }');
    var f = asmLink(c, this, null, ab);

    for (var i of indices) {
        var index = ((i<<scale)+disp)>>shift;
        var v = +arr[index];

        // Loads
        assertEq(f.load(i), v);

        // Stores
        arr[index] = ~v;
        f.store(i, v);
        assertEq(+arr[index], v);
    }
}

function testFloat32(ctor, shift, scale, disp) {
    testFloat(ctor, shift, scale, disp, "toF");
}
function testFloat64(ctor, shift, scale, disp) {
    testFloat(ctor, shift, scale, disp, "+");
}

function assertEqX4(observed, expected) {
    assertEq(observed.x, expected.x);
    assertEq(observed.y, expected.y);
    assertEq(observed.z, expected.z);
    assertEq(observed.w, expected.w);
}

function testSimdX4(ctor, shift, scale, disp, simdName, simdCtor) {
    var arr = new ctor(ab);

    var c = asmCompile('glob', 'imp', 'b',
                       USE_ASM +
                       'var arr=new glob.' + ctor.name + '(b); ' +
                       'var SIMD_' + simdName + ' = glob.SIMD.' + simdName + '; ' +
                       'var SIMD_' + simdName + '_check = SIMD_' + simdName + '.check; ' +
                       'var SIMD_' + simdName + '_load = SIMD_' + simdName + '.load; ' +
                       'var SIMD_' + simdName + '_loadXYZ = SIMD_' + simdName + '.loadXYZ; ' +
                       'var SIMD_' + simdName + '_loadXY = SIMD_' + simdName + '.loadXY; ' +
                       'var SIMD_' + simdName + '_loadX = SIMD_' + simdName + '.loadX; ' +
                       'var SIMD_' + simdName + '_store = SIMD_' + simdName + '.store; ' +
                       'var SIMD_' + simdName + '_storeXYZ = SIMD_' + simdName + '.storeXYZ; ' +
                       'var SIMD_' + simdName + '_storeXY = SIMD_' + simdName + '.storeXY; ' +
                       'var SIMD_' + simdName + '_storeX = SIMD_' + simdName + '.storeX; ' +
                       'function load(i) {i=i|0; return SIMD_' + simdName + '_check(SIMD_' + simdName + '_load(arr, ((i<<' + scale + ')+' + disp + ')>>' + shift + ')) } ' +
                       'function loadXYZ(i) {i=i|0; return SIMD_' + simdName + '_check(SIMD_' + simdName + '_loadXYZ(arr, ((i<<' + scale + ')+' + disp + ')>>' + shift + ')) } ' +
                       'function loadXY(i) {i=i|0; return SIMD_' + simdName + '_check(SIMD_' + simdName + '_loadXY(arr, ((i<<' + scale + ')+' + disp + ')>>' + shift + ')) } ' +
                       'function loadX(i) {i=i|0; return SIMD_' + simdName + '_check(SIMD_' + simdName + '_loadX(arr, ((i<<' + scale + ')+' + disp + ')>>' + shift + ')) } ' +
                       'function store(i,j) {i=i|0;j=SIMD_' + simdName + '_check(j); SIMD_' + simdName + '_store(arr, ((i<<' + scale + ')+' + disp + ')>>' + shift + ', j) } ' +
                       'function storeXYZ(i,j) {i=i|0;j=SIMD_' + simdName + '_check(j); SIMD_' + simdName + '_storeXYZ(arr, ((i<<' + scale + ')+' + disp + ')>>' + shift + ', j) } ' +
                       'function storeXY(i,j) {i=i|0;j=SIMD_' + simdName + '_check(j); SIMD_' + simdName + '_storeXY(arr, ((i<<' + scale + ')+' + disp + ')>>' + shift + ', j) } ' +
                       'function storeX(i,j) {i=i|0;j=SIMD_' + simdName + '_check(j); SIMD_' + simdName + '_storeX(arr, ((i<<' + scale + ')+' + disp + ')>>' + shift + ', j) } ' +
                       'return { load: load, loadXYZ: loadXYZ, loadXY: loadXY, loadX: loadX, store: store, storeXYZ: storeXYZ, storeXY : storeXY, storeX : storeX }');
    var f = asmLink(c, this, null, ab);

    for (var i of indices) {
        var index = ((i<<scale)+disp)>>shift;

        var v, vXYZ, vXY, vX;
        var t = false, tXYZ = false, tXY = false, tX = false;
        try { v = simdCtor.load(arr, index); }
        catch (e) {
            assertEq(e instanceof RangeError, true);
            t = true;
        }
        try { vXYZ = simdCtor.loadXYZ(arr, index); }
        catch (e) {
            assertEq(e instanceof RangeError, true);
            tXYZ = true;
        }
        try { vXY = simdCtor.loadXY(arr, index); }
        catch (e) {
            assertEq(e instanceof RangeError, true);
            tXY = true;
        }
        try { vX = simdCtor.loadX(arr, index); }
        catch (e) {
            assertEq(e instanceof RangeError, true);
            tX = true;
        }

        // Loads
        var l, lXYZ, lXY, lX;
        var r = false, rXYZ = false, rXY = false, rX = false;
        try { l = f.load(i); }
        catch (e) {
            assertEq(e instanceof RangeError, true);
            r = true;
        }
        try { lXYZ = f.loadXYZ(i); }
        catch (e) {
            assertEq(e instanceof RangeError, true);
            rXYZ = true;
        }
        try { lXY = f.loadXY(i); }
        catch (e) {
            assertEq(e instanceof RangeError, true);
            rXY = true;
        }
        try { lX = f.loadX(i); }
        catch (e) {
            assertEq(e instanceof RangeError, true);
            rX = true;
        }
        assertEq(t, r);
        assertEq(tXYZ, rXYZ);
        assertEq(tXY, rXY);
        assertEq(tX, rX);
        if (!t) assertEqX4(v, l);
        if (!tXYZ) assertEqX4(vXYZ, lXYZ);
        if (!tXY) assertEqX4(vXY, lXY);
        if (!tX) assertEqX4(vX, lX);

        // Stores
        if (!t) {
            simdCtor.store(arr, index, simdCtor.not(v));
            f.store(i, v);
            assertEqX4(simdCtor.load(arr, index), v);
        } else
            assertThrowsInstanceOf(() => f.store(i, simdCtor()), RangeError);
        if (!tXYZ) {
            simdCtor.storeXYZ(arr, index, simdCtor.not(vXYZ));
            f.storeXYZ(i, vXYZ);
            assertEqX4(simdCtor.loadXYZ(arr, index), vXYZ);
        } else
            assertThrowsInstanceOf(() => f.storeXYZ(i, simdCtor()), RangeError);
        if (!tXY) {
            simdCtor.storeXY(arr, index, simdCtor.not(vXY));
            f.storeXY(i, vXY);
            assertEqX4(simdCtor.loadXY(arr, index), vXY);
        } else
            assertThrowsInstanceOf(() => f.storeXY(i, simdCtor()), RangeError);
        if (!tX) {
            simdCtor.storeX(arr, index, simdCtor.not(vX));
            f.storeX(i, vX);
            assertEqX4(simdCtor.loadX(arr, index), vX);
        } else
            assertThrowsInstanceOf(() => f.storeX(i, simdCtor()), RangeError);
    }
}

function testFloat32x4(ctor, shift, scale, disp) {
    testSimdX4(ctor, shift, scale, disp, 'float32x4', SIMD.float32x4);
}
function testInt32x4(ctor, shift, scale, disp) {
    testSimdX4(ctor, shift, scale, disp, 'int32x4', SIMD.int32x4);
}

function test(tester, ctor, shift) {
    var arr = new ctor(ab);
    for (var i = 0; i < arr.length; i++)
        arr[i] = Math.imul(i, Math.imul((i & 1), 2) - 1);
    for (scale of [0,1,2,3]) {
        for (disp of [0,1,2,8,Math.pow(2,31)-1,Math.pow(2,31),Math.pow(2,32)-1])
            tester(ctor, shift, scale, disp);
    }
    for (var i = 0; i < arr.length; i++) {
        var v = arr[i];
        arr[i] = Math.imul(i, Math.imul((i & 1), 2) - 1);
        assertEq(arr[i], v);
    }
}

test(testInt, Int8Array, 0);
test(testInt, Uint8Array, 0);
test(testInt, Int16Array, 1);
test(testInt, Uint16Array, 1);
test(testInt, Int32Array, 2);
test(testInt, Uint32Array, 2);
test(testFloat32, Float32Array, 2);
test(testFloat64, Float64Array, 3);
if (typeof SIMD !== 'undefined' && isSimdAvailable()) {
    test(testInt32x4, Uint8Array, 0);
    test(testFloat32x4, Uint8Array, 0);
}
