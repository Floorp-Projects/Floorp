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
    var Float32x4 = global.SIMD.Float32x4;
    var splat = Float32x4.splat;
    var ext = Float32x4.extractLane;
    function e() {
        var v = Float32x4(0,0,0,0);
        var x = frd(0.);
        v = splat(.1e+71);
        x = ext(v,0);
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
    var Float32x4 = global.SIMD.Float32x4;
    var splat = Float32x4.splat;
    var ext = Float32x4.extractLane;
    function e() {
        return +ext(splat(.1e+71),0);
    }
    return e;
`), this)();

assertEq(v, Infinity);
