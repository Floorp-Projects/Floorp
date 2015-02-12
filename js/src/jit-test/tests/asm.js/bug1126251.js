load(libdir + "asm.js");

// Bug 1126251
var v = asmLink(asmCompile('global', `
    "use asm";
    var frd = global.Math.fround;
    function e() {
        var x = frd(.1e+71);
        x = frd(x / x);
        return +x;
    }
    return e;
`), this)();

assertEq(v, NaN);

if (!isSimdAvailable() || typeof SIMD === 'undefined') {
    quit(0);
}

var v = asmLink(asmCompile('global', `
    "use asm";
    var frd = global.Math.fround;
    var float32x4 = global.SIMD.float32x4;
    var splat = float32x4.splat;
    function e() {
        var v = float32x4(0,0,0,0);
        var x = frd(0.);
        v = splat(.1e+71);
        x = v.x;
        x = frd(x / x);
        return +x;
    }
    return e;
`), this)();

assertEq(v, NaN);

// Bug 1130618: without GVN
setJitCompilerOption("ion.gvn.enable", 0);
var v = asmLink(asmCompile('global', `
    "use asm";
    var float32x4 = global.SIMD.float32x4;
    var splat = float32x4.splat;
    function e() {
        return +splat(.1e+71).x;
    }
    return e;
`), this)();

assertEq(v, Infinity);
