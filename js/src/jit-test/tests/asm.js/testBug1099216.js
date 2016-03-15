if (typeof SIMD === 'undefined' || !isSimdAvailable()) {
    print("won't run tests as simd extensions aren't activated yet");
    quit(0);
}

(function(global) {
    "use asm";
    var frd = global.Math.fround;
    var fx4 = global.SIMD.Float32x4;
    var fc4 = fx4.check;
    var fsp = fx4.splat;
    function s(){}
    function d(x){x=fc4(x);}
    function e() {
        var x = frd(0);
        x = frd(x / x);
        s();
        d(fsp(x));
    }
    return e;
})(this)();

(function(m) {
    "use asm"
    var k = m.SIMD.Bool32x4
    var g = m.SIMD.Int32x4
    var gc = g.check;
    var h = g.select
    function f() {
        var x = k(0, 0, 0, 0)
        var y = g(1, 2, 3, 4)
        return gc(h(x, y, y))
    }
    return f;
})(this)();

t = (function(global) {
    "use asm"
    var toF = global.Math.fround
    var f4 = global.SIMD.Float32x4
    var f4c = f4.check
    function p(x, y, width, value, max_iterations) {
        x = x | 0
        y = y | 0
        width = width | 0
        value = value | 0
        max_iterations = max_iterations | 0
    }
    function m(xf, yf, yd, max_iterations) {
        xf = toF(xf)
        yf = toF(yf)
        yd = toF(yd)
        max_iterations = max_iterations | 0
        var _ = f4(0, 0, 0, 0), c_im4 = f4(0, 0, 0, 0)
        c_im4 = f4(yf, yd, yd, yf)
        return f4c(c_im4);
    }
    return {p:p,m:m};
})(this)
t.p();
t.m();
