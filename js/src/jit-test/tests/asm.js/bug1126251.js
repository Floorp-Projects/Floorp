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
