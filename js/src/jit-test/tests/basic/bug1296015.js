setJitCompilerOption("ion.warmup.trigger", 50);
setJitCompilerOption("offthread-compilation.enable", 0);
gcPreserveCode();

function f() {
    for (var i=0; i<30000; i++) {
        var a = inIon() ? 0 : 300;
        var buf = new Uint8ClampedArray(a);
        (function() {}) * this;
    }
    try {} catch(e) {}
}
f();
