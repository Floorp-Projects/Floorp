g = Function("", "for (var i = 0; i < 0; ++i) { eval('this.arg'+0 +'=arg'+0); }");
Math.abs(undefined);
gczeal(2,300);
evaluate("\
var toFloat32 = (function() {\
    var f32 = new Float32Array(1);\
    function f(x) f32[0] = x;\
    return f;\
})();\
for (var i = 0; i < 64; ++i) {\
    var p = Math.pow(2, i) + 1;\
    g(toFloat32(p));\
    toFloat32(-p);\
}");
