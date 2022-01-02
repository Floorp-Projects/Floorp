// |jit-test| skip-if: !isAsmJSCompilationAvailable()
load(libdir + "asm.js");
load(libdir + "asserts.js");

// This test runs a lot of code and is very slow with --ion-eager. Use a minimum
// Ion warmup trigger of 5 to avoid timeouts.
if (getJitCompilerOptions()["ion.warmup.trigger"] < 5)
    setJitCompilerOption("ion.warmup.trigger", 5);

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

function test(tester, ctor, shift) {
    var arr = new ctor(ab);
    for (var i = 0; i < arr.length; i++)
        arr[i] = Math.imul(i, Math.imul((i & 1), 2) - 1);
    for (scale of [0,1,2,3]) {
        for (disp of [0,1,2,8,Math.pow(2,30),Math.pow(2,31)-1,Math.pow(2,31),Math.pow(2,32)-1])
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
