
var buf = new ArrayBuffer(4096);
var f64 = new Float64Array(buf);
var i32 = new Int32Array(buf);
var u32 = new Uint32Array(buf);

function ffi(d) {
  str = String(d);
}


function FFI1(glob, imp, b) {
    "use asm";

    var i8=new glob.Int8Array(b);var u8=new glob.Uint8Array(b);
    var i16=new glob.Int16Array(b);var u16=new glob.Uint16Array(b);
    var i32=new glob.Int32Array(b);var u32=new glob.Uint32Array(b);
    var f32=new glob.Float32Array(b);var f64=new glob.Float64Array(b);

    var ffi=imp.ffi;

    function g() {
      ffi(+f64[0])
    }
    return g
}

g = FFI1(this, {ffi:ffi}, buf);


// that sounds dangerous!
var a = [0,1,0xffff0000,0x7fff0000,0xfff80000,0x7ff80000,0xfffc0000,0x7ffc0000,0xffffffff,0x0000ffff,0x00008fff7];
for (i of a) {
    for (j of a) {
        u32[0] = i;
        u32[1] = j;

        print(f64[0]+" (input)");
        //assertEq(f(), f64[0]);

        g();
        assertEq(str, String(f64[0]));
    }
}
