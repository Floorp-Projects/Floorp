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
                       'var SIMD_' + simdName + '_load3 = SIMD_' + simdName + '.load3; ' +
                       'var SIMD_' + simdName + '_load2 = SIMD_' + simdName + '.load2; ' +
                       'var SIMD_' + simdName + '_load1 = SIMD_' + simdName + '.load1; ' +
                       'var SIMD_' + simdName + '_store = SIMD_' + simdName + '.store; ' +
                       'var SIMD_' + simdName + '_store3 = SIMD_' + simdName + '.store3; ' +
                       'var SIMD_' + simdName + '_store2 = SIMD_' + simdName + '.store2; ' +
                       'var SIMD_' + simdName + '_store1 = SIMD_' + simdName + '.store1; ' +
                       'function load(i) {i=i|0; return SIMD_' + simdName + '_check(SIMD_' + simdName + '_load(arr, ((i<<' + scale + ')+' + disp + ')>>' + shift + ')) } ' +
                       'function load3(i) {i=i|0; return SIMD_' + simdName + '_check(SIMD_' + simdName + '_load3(arr, ((i<<' + scale + ')+' + disp + ')>>' + shift + ')) } ' +
                       'function load2(i) {i=i|0; return SIMD_' + simdName + '_check(SIMD_' + simdName + '_load2(arr, ((i<<' + scale + ')+' + disp + ')>>' + shift + ')) } ' +
                       'function load1(i) {i=i|0; return SIMD_' + simdName + '_check(SIMD_' + simdName + '_load1(arr, ((i<<' + scale + ')+' + disp + ')>>' + shift + ')) } ' +
                       'function store(i,j) {i=i|0;j=SIMD_' + simdName + '_check(j); SIMD_' + simdName + '_store(arr, ((i<<' + scale + ')+' + disp + ')>>' + shift + ', j) } ' +
                       'function store3(i,j) {i=i|0;j=SIMD_' + simdName + '_check(j); SIMD_' + simdName + '_store3(arr, ((i<<' + scale + ')+' + disp + ')>>' + shift + ', j) } ' +
                       'function store2(i,j) {i=i|0;j=SIMD_' + simdName + '_check(j); SIMD_' + simdName + '_store2(arr, ((i<<' + scale + ')+' + disp + ')>>' + shift + ', j) } ' +
                       'function store1(i,j) {i=i|0;j=SIMD_' + simdName + '_check(j); SIMD_' + simdName + '_store1(arr, ((i<<' + scale + ')+' + disp + ')>>' + shift + ', j) } ' +
                       'return { load: load, load3: load3, load2: load2, load1: load1, store: store, store3: store3, store2 : store2, store1 : store1 }');
    var f = asmLink(c, this, null, ab);

    for (var i of indices) {
        var index = ((i<<scale)+disp)>>shift;

        var v, v3, v2, v1;
        var t = false, t3 = false, t2 = false, t1 = false;
        try { v = simdCtor.load(arr, index); }
        catch (e) {
            assertEq(e instanceof RangeError, true);
            t = true;
        }
        try { v3 = simdCtor.load3(arr, index); }
        catch (e) {
            assertEq(e instanceof RangeError, true);
            t3 = true;
        }
        try { v2 = simdCtor.load2(arr, index); }
        catch (e) {
            assertEq(e instanceof RangeError, true);
            t2 = true;
        }
        try { v1 = simdCtor.load1(arr, index); }
        catch (e) {
            assertEq(e instanceof RangeError, true);
            t1 = true;
        }

        // Loads
        var l, l3, l2, l1;
        var r = false, r3 = false, r2 = false, r1 = false;
        try { l = f.load(i); }
        catch (e) {
            assertEq(e instanceof RangeError, true);
            r = true;
        }
        try { l3 = f.load3(i); }
        catch (e) {
            assertEq(e instanceof RangeError, true);
            r3 = true;
        }
        try { l2 = f.load2(i); }
        catch (e) {
            assertEq(e instanceof RangeError, true);
            r2 = true;
        }
        try { l1 = f.load1(i); }
        catch (e) {
            assertEq(e instanceof RangeError, true);
            r1 = true;
        }
        assertEq(t, r);
        assertEq(t3, r3);
        assertEq(t2, r2);
        assertEq(t1, r1);
        if (!t) assertEqX4(v, l);
        if (!t3) assertEqX4(v3, l3);
        if (!t2) assertEqX4(v2, l2);
        if (!t1) assertEqX4(v1, l1);

        // Stores
        if (!t) {
            simdCtor.store(arr, index, simdCtor.not(v));
            f.store(i, v);
            assertEqX4(simdCtor.load(arr, index), v);
        } else
            assertThrowsInstanceOf(() => f.store(i, simdCtor()), RangeError);
        if (!t3) {
            simdCtor.store3(arr, index, simdCtor.not(v3));
            f.store3(i, v3);
            assertEqX4(simdCtor.load3(arr, index), v3);
        } else
            assertThrowsInstanceOf(() => f.store3(i, simdCtor()), RangeError);
        if (!t2) {
            simdCtor.store2(arr, index, simdCtor.not(v2));
            f.store2(i, v2);
            assertEqX4(simdCtor.load2(arr, index), v2);
        } else
            assertThrowsInstanceOf(() => f.store2(i, simdCtor()), RangeError);
        if (!t1) {
            simdCtor.store1(arr, index, simdCtor.not(v1));
            f.store1(i, v1);
            assertEqX4(simdCtor.load1(arr, index), v1);
        } else
            assertThrowsInstanceOf(() => f.store1(i, simdCtor()), RangeError);
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
