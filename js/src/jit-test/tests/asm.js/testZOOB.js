load(libdir + "asm.js");

setIonCheckGraphCoherency(false);
setCachingEnabled(false);

// constants
var buf = new ArrayBuffer(BUF_MIN);

// An unshifted literal constant byte index in the range 0 to 2^31-1 inclusive should give a link failure.
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int8Array(b);  function f() {return arr[0x7fffffff]|0 } return f'), this, null, buf);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int32Array(b); function f() {return arr[0x1fffffff]|0 } return f'), this, null, buf);


// An unshifted literal constant byte index outside the range 0 to 2^31-1 inclusive should cause an error compiling.
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int32Array(b); function f() {return arr[0x20000000]|0 } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int32Array(b); function f() {return arr[0x3fffffff]|0 } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int32Array(b); function f() {return arr[0x40000000]|0 } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int32Array(b); function f() {return arr[0x7fffffff]|0 } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int32Array(b); function f() {return arr[0x80000000]|0 } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int32Array(b); function f() {return arr[0x8fffffff]|0 } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int32Array(b); function f() {return arr[0xffffffff]|0 } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int32Array(b); function f() {return arr[0x100000000]|0 } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int8Array(b);  function f() {return arr[0x80000000]|0 } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int8Array(b);  function f() {return arr[0xffffffff]|0 } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int8Array(b);  function f() {return arr[0x100000000]|0 } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int16Array(b); function f() {return arr[-1]|0 } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int32Array(b); function f() {return arr[-2]|0 } return f');

assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int32Array(b); function f() {return arr[10-12]|0 } return f');

// An intish shifted literal constant index should not fail to compile or link.
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int8Array(b);  function f() {return arr[0x3fffffff>>0]|0 } return f'), this, null, buf)(), 0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int32Array(b); function f() {return arr[0x3fffffff>>2]|0 } return f'), this, null, buf)(), 0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int8Array(b);  function f() {return arr[0xffffffff>>0]|0 } return f'), this, null, buf)(), 0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int32Array(b); function f() {return arr[0xffffffff>>2]|0 } return f'), this, null, buf)(), 0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int8Array(b);  function f() {return arr[-1>>0]|0 } return f'), this, null, buf)(), 0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int32Array(b); function f() {return arr[-1>>2]|0 } return f'), this, null, buf)(), 0);
// Unsigned (intish) folded constant index.
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int8Array(b);  function f() {return arr[0xffffffff>>>0]|0 } return f'), this, null, buf)(), 0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int8Array(b);  function f() {arr[0] = 1; return arr[(0xffffffff+1)>>>0]|0 } return f'), this, null, buf)(), 1);

// A non-intish shifted literal constant index should cause an error compiling.
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int8Array(b); function f() {return arr[0x100000000>>0]|0 } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int32Array(b); function f() {return arr[0x100000000>>2]|0 } return f');

// Folded non-intish constant expressions should cause an error compiling.
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.Int8Array(b);  function f() {return arr[0xffffffff+1]|0 } return f');

var ab = new ArrayBuffer(BUF_MIN);
var arr = new Int32Array(BUF_MIN);
for (var i = 0; i < arr.length; i++)
    arr[i] = i;

function testInt(ctor, shift, scale, disp) {
    var arr = new ctor(ab);
    var f = asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.' + ctor.name + '(b); function f(i) {i=i|0; return arr[((i<<' + scale + ')+' + disp + ')>>' + shift + ']|0 } return f'), this, null, ab);
    for (var i of [0,1,2,3,4,1023,1024,1025,BUF_MIN-2,BUF_MIN-1,BUF_MIN,BUF_MIN+1])
        assertEq(f(i), arr[((i<<scale)+disp)>>shift]|0);

    for (var i of [-Math.pow(2,28),Math.pow(2,28),-Math.pow(2,29),Math.pow(2,29),-Math.pow(2,30),Math.pow(2,30),-Math.pow(2,31),Math.pow(2,31),-Math.pow(2,32),Math.pow(2,32)]) {
        for (var j of [-8,-4,-1,0,1,4,8])
            assertEq(f(i+j), arr[(((i+j)<<scale)+disp)>>shift]|0);
    }

    var f = asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.' + ctor.name + '(b); function f(i,j) {i=i|0;j=j|0; arr[((i<<' + scale + ')+' + disp + ')>>' + shift + '] = j } return f'), this, null, ab);
    for (var i of [0,1,2,3,4,1023,1024,1025,4095,4096,4097]) {
        var index = ((i<<scale)+disp)>>shift;
        var v = arr[index]|0;
        arr[index] = 0;
        f(i, v);
        assertEq(arr[index]|0, v);
    }

    for (var i of [-Math.pow(2,31), Math.pow(2,31)-1, Math.pow(2,32)]) {
        for (var j of [-8,-4,-1,0,1,4,8]) {
            var index = (((i+j)<<scale)+disp)>>shift;
            var v = arr[index]|0;
            arr[index] = 0;
            f(i+j, v);
            assertEq(arr[index]|0, v);
        }
    }
}

function testFloat(ctor, shift, scale, disp, coercion) {
    var arr = new ctor(ab);
    var f = asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.' + ctor.name + '(b); var toF = glob.Math.fround; function f(i) {i=i|0; return ' + coercion + '(arr[((i<<' + scale + ')+' + disp + ')>>' + shift + ']) } return f'), this, null, ab);
    for (var i of [0,1,2,3,4,1023,1024,1025,BUF_MIN-2,BUF_MIN-1,BUF_MIN,BUF_MIN+1])
        assertEq(f(i), +arr[((i<<scale)+disp)>>shift]);

    for (var i of [-Math.pow(2,31), Math.pow(2,31)-1, Math.pow(2,32)]) {
        for (var j of [-8,-4,-1,0,1,4,8])
            assertEq(f(i+j), +arr[(((i+j)<<scale)+disp)>>shift]);
    }

    var f = asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + 'var arr=new glob.' + ctor.name + '(b); var toF = glob.Math.fround; function f(i,j) {i=i|0;j=+j; arr[((i<<' + scale + ')+' + disp + ')>>' + shift + '] = j } return f'), this, null, ab);
    for (var i of [0,1,2,3,4,1023,1024,1025,4095,4096,4097]) {
        var index = ((i<<scale)+disp)>>shift;
        var v = +arr[index];
        arr[index] = 0;
        f(i, v);
        assertEq(+arr[index], v);
    }

    for (var i of [-Math.pow(2,31), Math.pow(2,31)-1, Math.pow(2,32)]) {
        for (var j of [-8,-4,-1,0,1,4,8]) {
            var index = (((i+j)<<scale)+disp)>>shift;
            var v = +arr[index];
            arr[index] = 0;
            f(i+j, v);
            assertEq(+arr[index], v);
        }
    }
}

function testFloat32(ctor, shift, scale, disp) {
    testFloat(ctor, shift, scale, disp, "toF");
}
function testFloat64(ctor, shift, scale, disp) {
    testFloat(ctor, shift, scale, disp, "+");
}

function test(tester, ctor, shift) {
    for (scale of [0,1,2,3]) {
        for (disp of [0,1,8,Math.pow(2,31)-1,Math.pow(2,31),Math.pow(2,32)-1])
            tester(ctor, shift, scale, disp);
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
